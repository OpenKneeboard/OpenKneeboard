#include <OpenKneeboard/bitflags.h>
#include <OpenKneeboard/shm.h>
#include <Windows.h>
#include <fmt/compile.h>
#include <fmt/format.h>

#include <bit>

namespace OpenKneeboard::SHM {

// *****PLEASE***** change this if you fork or re-use this code
static constexpr auto PREFIX = "com.fredemmott.openkneeboard";

enum class HeaderFlags : ULONG {
  LOCKED = 1 << 0,
  FEEDER_ATTACHED = 1 << 1,
};

#pragma pack(push)
struct Header final {
  static constexpr uint16_t VERSION = 1;

  uint32_t sequenceNumber = 0;
  HeaderFlags flags;
  Config config;
};
#pragma pack(pop)

}// namespace OpenKneeboard::SHM

namespace OpenKneeboard {
template <>
constexpr bool is_bitflags_v<SHM::HeaderFlags> = true;
}// namespace OpenKneeboard

namespace OpenKneeboard::SHM {

namespace {

class Spinlock final {
 private:
  Header* mHeader = nullptr;

 public:
  enum FAILURE_BEHAVIOR {
    ON_FAILURE_FORCE_UNLOCK,
    ON_FAILURE_CREATE_FALSEY,
    ON_FAILURE_THROW_EXCEPTION
  };

  Spinlock(Header* header, FAILURE_BEHAVIOR behavior) : mHeader(header) {
    const auto bit = std::countr_zero(static_cast<ULONG>(HeaderFlags::LOCKED));
    static_assert(static_cast<ULONG>(HeaderFlags::LOCKED) == 1 << bit);

    for (int count = 1; count <= 1000; ++count) {
      auto alreadyLocked
        = _interlockedbittestandset(std::bit_cast<LONG*>(&header->flags), bit);
      if (!alreadyLocked) {
        return;
      }
      if (count % 10 == 0) {
        YieldProcessor();
      }
      if (count % 100 == 0) {
        Sleep(10 /* ms */);
      }
    }

    if (behavior == ON_FAILURE_FORCE_UNLOCK) {
      return;
    }

    if (behavior == ON_FAILURE_CREATE_FALSEY) {
      mHeader = nullptr;
      return;
    }

    throw new std::runtime_error("Failed to acquire spinlock");
  }

  operator bool() {
    return (bool)mHeader;
  }

  ~Spinlock() {
    if (!mHeader) {
      return;
    }

    mHeader->flags ^= HeaderFlags::LOCKED;
  }
};

constexpr DWORD MAX_IMAGE_PX(1024 * 1024 * 8);
constexpr DWORD MAX_IMAGE_BYTES = MAX_IMAGE_PX * sizeof(Pixel);
constexpr DWORD SHM_SIZE = sizeof(Header) + MAX_IMAGE_BYTES;

constexpr auto SHMPath() {
  char buf[255];
  auto end = fmt::format_to(
    buf,
    FMT_COMPILE("{}/h{}-c{}-vrc{}-fc{}-s{:x}"),
    PREFIX,
    Header::VERSION,
    Config::VERSION,
    VRConfig::VERSION,
    FlatConfig::VERSION,
    SHM_SIZE);
  return std::string(buf, end);
}

}// namespace

Snapshot::Snapshot() {
}

Snapshot::Snapshot(std::vector<std::byte>&& bytes) {
  mBytes = std::make_shared<std::vector<std::byte>>(std::move(bytes));
}

uint32_t Snapshot::GetSequenceNumber() const {
  if (!mBytes) {
    return 0;
  }
  return reinterpret_cast<const Header*>(mBytes->data())->sequenceNumber;
}

const Config* const Snapshot::GetConfig() const {
  if (!mBytes) {
    return nullptr;
  }
  return &reinterpret_cast<const Header*>(mBytes->data())->config;
}

const Pixel* const Snapshot::GetPixels() const {
  if (!mBytes) {
    return nullptr;
  }
  return reinterpret_cast<const Pixel*>(&mBytes->data()[sizeof(Header)]);
}

Snapshot::operator bool() const {
  return (bool)mBytes;
}

class Impl {
 public:
  HANDLE Handle;
  std::byte* Mapping = nullptr;
  Header* Header = nullptr;
  Pixel* Pixels = nullptr;
  bool IsFeeder;

  Snapshot Latest;

  ~Impl() {
    if (IsFeeder) {
      Header->flags ^= HeaderFlags::FEEDER_ATTACHED;
      FlushViewOfFile(Mapping, NULL);
    }
    UnmapViewOfFile(Mapping);
    CloseHandle(Handle);
  }
};

Writer::Writer() {
  const auto path = SHMPath();

  HANDLE handle;
  handle = CreateFileMappingA(
    INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, path.c_str());
  if (!handle) {
    return;
  }
  auto mapping = reinterpret_cast<std::byte*>(
    MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, SHM_SIZE));
  if (!mapping) {
    CloseHandle(handle);
    return;
  }

  this->p.reset(new Impl {
    .Handle = handle,
    .Mapping = mapping,
    .Header = reinterpret_cast<Header*>(mapping),
    .Pixels = reinterpret_cast<Pixel*>(&mapping[sizeof(Header)]),
    .IsFeeder = true});
}

Writer::~Writer() {
}

Reader::Reader() {
  const auto path = SHMPath();
  HANDLE handle;
  handle = CreateFileMappingA(
    INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, path.c_str());
  if (!handle) {
    return;
  }
  auto mapping = reinterpret_cast<std::byte*>(
    MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, SHM_SIZE));
  if (!mapping) {
    CloseHandle(handle);
    return;
  }

  this->p.reset(new Impl {
    .Handle = handle,
    .Mapping = mapping,
    .Header = reinterpret_cast<Header*>(mapping),
    .Pixels = reinterpret_cast<Pixel*>(&mapping[sizeof(Header)]),
    .IsFeeder = false});
}

Reader::~Reader() {
}

Reader::operator bool() const {
  return p && (p->Header->flags & HeaderFlags::FEEDER_ATTACHED);
}

Writer::operator bool() const {
  return (bool)p;
}

Snapshot Reader::MaybeGet() const {
  if (!(*this)) {
    return {};
  }

  const auto snapshot = p->Latest;

  if (snapshot && snapshot.GetSequenceNumber() == p->Header->sequenceNumber) {
    return snapshot;
  }

  std::vector<std::byte> buffer(SHM_SIZE);
  {
    Spinlock lock(p->Header, Spinlock::ON_FAILURE_CREATE_FALSEY);
    if (!lock) {
      return snapshot;
    }
    memcpy(reinterpret_cast<void*>(buffer.data()), p->Mapping, SHM_SIZE);
  }
  p->Latest = Snapshot(std::move(buffer));

  return p->Latest;
}

void Writer::Update(const Config& config, const std::vector<Pixel>& pixels) {
  if (!p) {
    throw std::logic_error("Attempted to update invalid SHM");
  }

  if (pixels.size() != config.imageWidth * config.imageHeight) {
    throw std::logic_error("Pixel array size does not match header");
  }

  Spinlock lock(p->Header, Spinlock::ON_FAILURE_FORCE_UNLOCK);
  p->Header->flags |= HeaderFlags::FEEDER_ATTACHED;
  p->Header->sequenceNumber++;
  p->Header->config = config;

  memcpy(
    reinterpret_cast<void*>(p->Pixels),
    pixels.data(),
    pixels.size() * sizeof(Pixel));
}

}// namespace OpenKneeboard::SHM
