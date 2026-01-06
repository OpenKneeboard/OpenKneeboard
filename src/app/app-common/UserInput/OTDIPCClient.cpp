// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/OTDIPCClient.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/task/resume_after.hpp>
#include <OpenKneeboard/task/resume_on_signal.hpp>

#include <KnownFolders.h>

#include <wil/cppwinrt.h>

#include <fstream>
#include <ranges>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>

#include <wil/cppwinrt_helpers.h>

#include <OTD-IPC/DebugMessage.hpp>
#include <OTD-IPC/DeviceInfo.hpp>
#include <OTD-IPC/MessageType.hpp>
#include <OTD-IPC/State.hpp>

using namespace OTDIPC::Messages;

namespace OpenKneeboard {

namespace {

std::vector<std::filesystem::path> GetSocketPaths() try {
  std::unordered_map<std::string, std::filesystem::path> socketsByID;
  const auto discoveryRoot
    = Filesystem::GetKnownFolderPath<FOLDERID_LocalAppData>() / "otd-ipc"
    / "servers" / "v2";
  for (auto&& entry:
       std::filesystem::directory_iterator(discoveryRoot / "available")) {
    if (!is_regular_file(entry)) {
      continue;
    }
    if (entry.path().extension() != ".txt") {
      continue;
    }
    std::ifstream file {entry.path()};
    std::stringstream buf;
    buf << file.rdbuf();
    const auto str = buf.str();
    struct {
      std::string_view mID;
      std::string_view mSocket;
      std::string_view mName;
      std::string_view mDebugVersion;
    } metadata;
    for (auto&& it: std::views::split(std::string_view {str}, '\n')) {
      std::string_view line {it};
      if (line.starts_with("ID=")) {
        line.remove_prefix(3);
        metadata.mID = line;
        continue;
      }
      if (line.starts_with("SOCKET=")) {
        line.remove_prefix(7);
        metadata.mSocket = line;
        continue;
      }
      if (line.starts_with("NAME=")) {
        line.remove_prefix(5);
        metadata.mName = line;
        continue;
      }
      if (line.starts_with("DEBUG_VERSION=")) {
        line.remove_prefix(14);
        metadata.mDebugVersion = line;
        continue;
      }
    }
    const auto metadataPath = entry.path().string();
    if (metadata.mID.empty()) {
      dprint.Warning("No ID found in `{}`", metadataPath);
      continue;
    }
    if (metadata.mSocket.empty()) {
      dprint.Warning("No socket path found in `{}`", metadataPath);
      continue;
    }
    dprint(
      "OTD-IPC server discovered:\n"
      "  Metadata: {}\n"
      "  ID: {}\n"
      "  Name: {}\n"
      "  Debug version: {}\n"
      "  Socket path: {}",
      metadataPath,
      metadata.mID,
      metadata.mName,
      metadata.mDebugVersion,
      metadata.mSocket);
    const std::string id {metadata.mID};
    if (socketsByID.contains(id)) {
      dprint.Warning(
        "Duplicate OTD-IPC server discovered with ID `{}`", metadata.mID);
      continue;
    }
    if (const auto basename = entry.path().stem().string(); basename != id) {
      dprint.Warning(
        "OTD-IPC server discovered with ID `{}` has basename `{}`, expected "
        "`{}`",
        metadata.mID,
        basename,
        id);
      continue;
    }
    socketsByID.emplace(std::move(id), metadata.mSocket);
  }

  if (socketsByID.empty()) {
    dprint.Warning("No OTD-IPC servers discovered");
    return {};
  }
  const auto defaultsPath = discoveryRoot / "default.txt";
  std::vector<std::filesystem::path> orderedPaths;
  orderedPaths.reserve(socketsByID.size());

  if (std::filesystem::exists(defaultsPath)) {
    std::ifstream file {defaultsPath};
    std::stringstream buf;
    buf << file.rdbuf();
    auto defaultID = buf.str();
    erase_if(defaultID, &isspace);
    if (defaultID.empty()) {
      dprint.Warning(
        "Empty default OTD-IPC server metadata found at `{}`",
        defaultsPath.string());
    } else if (const auto it = socketsByID.find(defaultID);
               it != socketsByID.end()) {
      dprint("Matched default OTD-IPC server ID `{}`", defaultID);
      orderedPaths.emplace_back(it->second);
      socketsByID.erase(it);
    } else {
      dprint.Warning(
        "Failed to match default OTD-IPC server ID `{}`", defaultID);
    }
  } else {
    dprint.Warning(
      "No default OTD-IPC server metadata found at `{}`",
      defaultsPath.string());
  }
  for (auto&& path: socketsByID | std::views::values) {
    orderedPaths.emplace_back(path);
  }
  return orderedPaths;
} catch (const std::filesystem::filesystem_error&) {
  return {};
}

template <std::size_t N>
std::string_view StringViewFromFixedSizedBuffer(const char (&buffer)[N]) {
  return std::string_view {&buffer[0], strnlen_s(buffer, N)};
}

struct InsufficientDataSocketReadError {
  std::size_t mExpected {};
  std::size_t mReceived {};
};
struct WouldBlockSocketReadError {};
struct SocketClosedSocketReadError {};
using SocketReadError = std::variant<
  WouldBlockSocketReadError,
  InsufficientDataSocketReadError,
  SocketClosedSocketReadError,
  HRESULT>;

std::expected<std::size_t, SocketReadError>
ReadFromSocket(SOCKET s, void* buf, const std::size_t bytesToRead) {
  if (bytesToRead == 0) {
    return 0;
  }

  const auto socketReadResult
    = recv(s, reinterpret_cast<char*>(buf), static_cast<int>(bytesToRead), 0);
  if (socketReadResult == 0) {
    return std::unexpected {SocketClosedSocketReadError {}};
  }
  if (socketReadResult == SOCKET_ERROR) {
    const auto err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
      return std::unexpected {WouldBlockSocketReadError {}};
    }
    return std::unexpected {HRESULT_FROM_WIN32(err)};
  }

  OPENKNEEBOARD_ALWAYS_ASSERT(socketReadResult > 0);
  const auto bytesRead = static_cast<std::size_t>(socketReadResult);

  if (bytesRead != bytesToRead) {
    return std::unexpected {
      InsufficientDataSocketReadError {bytesToRead, bytesRead}};
  }

  return bytesRead;
}

auto WaitForReadFromSocket(
  SOCKET s,
  void* buf,
  const std::size_t size,
  HANDLE event) -> task<decltype(ReadFromSocket(s, buf, size))> {
  auto ret = ReadFromSocket(s, buf, size);
  while (!ret) {
    if (!holds_alternative<WouldBlockSocketReadError>(ret.error())) {
      co_return ret;
    }
    co_await resume_on_signal(event, {});
    ret = ReadFromSocket(s, buf, size);
  }
  co_return ret;
}

void LogSocketReadError(const SocketReadError& err) {
  if (std::holds_alternative<SocketClosedSocketReadError>(err)) {
    dprint("OTD-IPC connection closed by server");
    return;
  }
  if (const auto it = std::get_if<InsufficientDataSocketReadError>(&err); it) {
    dprint.Warning(
      "Insufficient data read from OTD-IPC socket: expected {}, received "
      "{}",
      it->mExpected,
      it->mReceived);
    return;
  }
  if (const auto it = std::get_if<HRESULT>(&err); it) {
    dprint.Warning("OTD-IPC socket read error: {}", winrt::hresult(*it));
    return;
  }

  dprint.Error("Unrecognized error reading from OTD-IPC socket");
}
}// namespace

std::shared_ptr<OTDIPCClient> OTDIPCClient::Create() {
  auto ret = std::shared_ptr<OTDIPCClient>(new OTDIPCClient());
  ret->mRunner = ret->Run();
  return ret;
}

OTDIPCClient::OTDIPCClient() {
  dprint("{}", __FUNCTION__);
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
}

OTDIPCClient::~OTDIPCClient() {
  dprint("{}", __FUNCTION__);
}

task<void> OTDIPCClient::DisposeAsync() noexcept {
  OPENKNEEBOARD_TraceLoggingCoro("OTDIPCClient::DisposeAsync()");
  const auto disposing = co_await mDisposal.StartOnce();
  if (!disposing) {
    co_return;
  }
  dprint("Requesting OTDIPCClient stop");
  mStopper.request_stop();
  dprint("Waiting for OTDIPCClient coro");
  co_await std::move(mRunner).value();
  dprint("Shutting down OTDIPCClient DQC");
  co_await mDQC.ShutdownQueueAsync();
  dprint("OTDIPCClient::DisposeAsync() is complete");
}

task<void> OTDIPCClient::Run() {
  OPENKNEEBOARD_TraceLoggingCoro("OTDIPCClient::Run()");
  dprint("Starting OTD-IPC client");
  const scope_exit exitMessage([n = std::uncaught_exceptions()]() {
    if (std::uncaught_exceptions() > n) {
      dprint.Warning("Ending OTDIPClient::Run() with uncaught exceptions");
    } else {
      dprint("Ending OTDIPCClient::Run");
    }
  });
  auto workThread = mDQC.DispatcherQueue();
  co_await wil::resume_foreground(workThread);
  SetThreadDescription(GetCurrentThread(), L"OTD-IPC Client Thread");

  while (!mStopper.stop_requested()) {
    co_await this->RunSingle();
    co_await resume_after(std::chrono::seconds(1), mStopper.get_token());
  }
}

void OTDIPCClient::TimeoutTablet(uint32_t id) {
  mTabletsToTimeout.erase(id);

  const auto it = mTablets.find(id);
  if (it == mTablets.end()) {
    return;
  }

  auto& state = it->second.mState;
  if (!state) {
    return;
  }
  state->mIsActive = false;
  evTabletInputEvent.Emit(it->second.mDevice.mDevicePersistentID, *state);
}

void OTDIPCClient::TimeoutTablets() {
  const auto now = TimeoutClock::now();
  while (!mTabletsToTimeout.empty()) {
    const auto first = mTabletsToTimeout.begin();
    auto firstID = first->first;
    auto firstTimeout = first->second;

    for (const auto& [id, timeout]: mTabletsToTimeout) {
      if (timeout < firstTimeout) {
        firstID = id;
        firstTimeout = timeout;
      }
    }

    if (firstTimeout > now) {
      break;
    }
    TimeoutTablet(firstID);
  }
}

task<void> OTDIPCClient::RunSingle() {
  OPENKNEEBOARD_TraceLoggingCoro("OTDIPCClient::RunSingle()");
  const auto socketPaths = GetSocketPaths();
  if (socketPaths.empty()) {
    co_return;
  }

  // Initialize Winsock (limited to this scope)
  WSADATA wsaData {};
  const auto wsaStartup = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaStartup != 0) {
    dprint("OTD-IPC WSAStartup failed: {}", wsaStartup);
    co_return;
  }
  const scope_exit wsaCleanup([]() { WSACleanup(); });

  SOCKET sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    dprint("OTD-IPC socket(AF_UNIX) failed: {}", WSAGetLastError());
    co_return;
  }
  const scope_exit closeSock([&]() {
    if (sock != INVALID_SOCKET) {
      closesocket(sock);
    }
  });

  sockaddr_un addr {};
  addr.sun_family = AF_UNIX;
  bool connected = false;
  for (auto&& path: socketPaths) {
    const auto buf = path.string();
    memset(&addr.sun_path, 0, sizeof(addr.sun_path));
    strncpy_s(addr.sun_path, buf.data(), buf.size());
    if (
      connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
      == SOCKET_ERROR) {
      dprint.Warning(
        "Failed to connect to OTD-IPC socket `{}`: {}",
        buf,
        HRESULT_FROM_WIN32(WSAGetLastError()));
      continue;
    }
    dprint("Connected to OTD-IPC server (AF_UNIX) at `{}`", buf);
    connected = true;
    break;
  }

  if (!connected) {
    dprint.Warning("Failed to connect to any OTD-IPC socket");
    co_return;
  }

  const scope_exit exitMessage([]() {
    dprint(
      "Tearing down OTD-IPC connection with {} uncaught exceptions",
      std::uncaught_exceptions());
  });

  char buffer[1024];
  using namespace OTDIPC::Messages;
  const auto header = reinterpret_cast<const Header* const>(buffer);
  static_assert(sizeof(buffer) >= sizeof(DeviceInfo));
  static_assert(sizeof(buffer) >= sizeof(State));

  const auto event = Win32::CreateEvent(nullptr, false, false, nullptr).value();

  while (true) {
    if (mStopper.stop_requested()) {
      dprint("OTD-IPC task cancelled.");
      co_return;
    }

    this->TimeoutTablets();
    const auto waitDuration = mTabletsToTimeout.empty()
      ? std::chrono::milliseconds(1500)
      : std::ranges::min_element(mTabletsToTimeout)->second
        - TimeoutClock::now();

    WSAEventSelect(sock, event.get(), FD_READ | FD_WRITE | FD_CLOSE);

    if (!co_await winrt::resume_on_signal(
          event.get(),
          std::chrono::duration_cast<std::chrono::milliseconds>(
            waitDuration))) {
      for (auto&& id: std::exchange(mTabletsToTimeout, {}) | std::views::keys) {
        TimeoutTablet(id);
      }
      continue;
    }

    const auto headerBytes = ReadFromSocket(sock, buffer, sizeof(Header));
    if (!headerBytes) {
      if (std::holds_alternative<WouldBlockSocketReadError>(
            headerBytes.error())) {
        continue;
      }
      LogSocketReadError(headerBytes.error());
      co_return;
    }
    if (header->size < sizeof(Header)) {
      dprint.Warning(
        "OTD-IPC message size {} is smaller than header size", header->size);
      co_return;
    }
    if (header->size > sizeof(buffer)) {
      dprint.Warning(
        "OTD-IPC message size {} is larger than buffer size", header->size);
      co_return;
    }

    const auto bodyBytes = co_await WaitForReadFromSocket(
      sock,
      buffer + sizeof(Header),
      header->size - sizeof(Header),
      event.get());
    if (!bodyBytes) {
      LogSocketReadError(bodyBytes.error());
      co_return;
    }

    this->EnqueueMessage({buffer, header->size});
  }
}

OpenKneeboard::fire_and_forget OTDIPCClient::EnqueueMessage(
  const std::string message) {
  const auto weakThis = weak_from_this();
  co_await mUIThread;
  const auto self = weakThis.lock();
  if (!self) {
    co_return;
  }
  this->ProcessMessage(
    reinterpret_cast<const OTDIPC::Messages::Header&>(*message.data()));
}

template <class T, class TTablet>
std::optional<TTablet*> OTDIPCClient::GetTabletFromPersistentID(
  this T& self,
  const std::string& persistentID) {
  const auto id = self.mTabletIDs.find(persistentID);
  if (id == self.mTabletIDs.end()) {
    return std::nullopt;
  }
  const auto tablet = self.mTablets.find(id->second);
  if (tablet == self.mTablets.end()) {
    return std::nullopt;
  }
  return &tablet->second;
}

std::optional<TabletState> OTDIPCClient::GetState(
  const std::string& persistentID) const {
  const auto tablet = GetTabletFromPersistentID(persistentID);
  return tablet.and_then(&Tablet::mState);
}

std::optional<TabletInfo> OTDIPCClient::GetTablet(
  const std::string& persistentID) const {
  const auto tablet = GetTabletFromPersistentID(persistentID);
  if (!tablet) {
    return std::nullopt;
  }
  return (*tablet)->mDevice;
}

std::vector<TabletInfo> OTDIPCClient::GetTablets() const {
  std::vector<TabletInfo> ret;
  for (const auto& devices: mTablets | std::views::values) {
    ret.push_back(devices.mDevice);
  }
  return ret;
}

void OTDIPCClient::ProcessMessage(const OTDIPC::Messages::Header& header) {
  switch (header.messageType) {
    case MessageType::DeviceInfo:
      this->ProcessMessage(reinterpret_cast<const DeviceInfo&>(header));
      return;
    case MessageType::State:
      this->ProcessMessage(reinterpret_cast<const State&>(header));
      return;
    case MessageType::DebugMessage:
      this->ProcessMessage(reinterpret_cast<const DebugMessage&>(header));
      return;
    case MessageType::Ping:
      // nothing to do
      return;
  }
}

void OTDIPCClient::ProcessMessage(const OTDIPC::Messages::DeviceInfo& msg) {
  if (msg.size < sizeof(DeviceInfo)) {
    return;
  }

  TabletInfo info {
    .mMaxX = msg.maxX,
    .mMaxY = msg.maxY,
    .mMaxPressure = msg.maxPressure,
    .mDeviceName = std::string {StringViewFromFixedSizedBuffer(msg.name)},
  };
  info.mDevicePersistentID
    = std::string {StringViewFromFixedSizedBuffer(msg.persistentId)};

  dprint(
    "Received OTD-IPC device: '{}' - {}",
    info.mDeviceName,
    info.mDevicePersistentID);

  mTablets[msg.nonPersistentTabletId].mDevice = info;
  mTabletIDs[info.mDevicePersistentID] = msg.nonPersistentTabletId;
  evDeviceInfoReceivedEvent.Emit(info);
}

void OTDIPCClient::ProcessMessage(const OTDIPC::Messages::State& msg) {
  if (msg.size < sizeof(State)) {
    return;
  }

  const auto it = mTablets.find(msg.nonPersistentTabletId);
  if (it == mTablets.end()) {
    OPENKNEEBOARD_TraceLoggingWrite(
      "BadOTDIPCTabletID",
      TraceLoggingValue(msg.nonPersistentTabletId, "tabletID"));
    static std::once_flag sLogged;
    std::call_once(sLogged, [&msg]() {
      dprint.Warning(
        "OTD-IPC server sent state for unrecognized tablet ID {} (logged once "
        "only)",
        msg.nonPersistentTabletId);
    });
    OPENKNEEBOARD_BREAK;
    return;
  }

  auto& tablet = it->second;

  if (!tablet.mState) {
    dprint(
      "Received first packet for OTD-IPC device {}", msg.nonPersistentTabletId);
    tablet.mState = TabletState {};
  }
  auto& state = tablet.mState.value();

  using Bits = OTDIPC::Messages::State::ValidMask;
  if (msg.HasData(Bits::Position)) {
    state.mX = msg.x;
    state.mY = msg.y;
  }

  if (msg.HasData(Bits::Pressure)) {
    state.mPressure = msg.pressure;
    if (state.mPressure > 0) {
      state.mPenButtons |= 1;
    } else {
      state.mPenButtons &= ~1;
    }
  }

  if (msg.HasData(Bits::PenButtons)) {
    // Preserve tip button
    state.mPenButtons &= 1;
    state.mPenButtons |= (msg.penButtons << 1);
  }

  if (msg.HasData(Bits::AuxButtons)) {
    state.mAuxButtons = msg.auxButtons;
  }

  if (msg.HasData(Bits::PenIsNearSurface)) {
    // e.g. Wacom
    state.mIsActive = msg.penIsNearSurface;
  } else if (msg.HasData(Bits::Position)) {
    // e.g. Huion does not have proximity
    state.mIsActive = true;
    mTabletsToTimeout[msg.nonPersistentTabletId]
      = TimeoutClock::now() + std::chrono::milliseconds(100);
  }

  OPENKNEEBOARD_TraceLoggingScope("OTDIPCClient::evTabletInputEvent");
  evTabletInputEvent.Emit(tablet.mDevice.mDevicePersistentID, state);
}

void OTDIPCClient::ProcessMessage(const OTDIPC::Messages::DebugMessage& msg) {
  dprint("Debug message from OTD-IPC server: {}", msg.message());
}

}// namespace OpenKneeboard
