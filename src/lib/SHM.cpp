#include "OpenKneeboard/shm.h"

#include <Windows.h>
#include <fmt/format.h>

namespace OpenKneeboard::SHM {

using namespace OpenKneeboard::Flags;

static const DWORD MAX_IMAGE_PX(1024 * 1024 * 8);
static const DWORD MAX_IMAGE_BYTES = MAX_IMAGE_PX * sizeof(Pixel);
static const DWORD SHM_SIZE = sizeof(Header) + MAX_IMAGE_BYTES;
static constexpr uint64_t SHM_VERSION
  = (uint64_t(Header::VERSION) << 32) | SHM_SIZE;
// *****PLEASE***** change this if you fork or re-use this code
static const auto SHM_PREFIX = "com.fredemmott.openkneeboard";

Snapshot::Snapshot() {
}

Snapshot::Snapshot(std::vector<std::byte>&& bytes) {
  mBytes = std::make_shared<std::vector<std::byte>>(std::move(bytes));
}

const Header* const Snapshot::GetHeader() const {
  if (!mBytes) {
    return nullptr;
  }
  return reinterpret_cast<const Header*>(mBytes->data());
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
  void* Mapping;
  Header* Header;
  Pixel* Pixels;
  bool IsFeeder;

  Snapshot Latest;

  ~Impl() {
    if (IsFeeder) {
      Header->Flags &= ~Flags::FEEDER_ATTACHED;
      FlushViewOfFile(Mapping, NULL);
    }
    UnmapViewOfFile(Mapping);
    CloseHandle(Handle);
  }
};

Writer::Writer() {
  const auto path = fmt::format("{}/{:x}", SHM_PREFIX, SHM_VERSION);
  HANDLE handle;
  handle = CreateFileMappingA(
    INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, path.c_str());
  if (!handle) {
    return;
  }
  void* mapping = MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, SHM_SIZE);
  if (!mapping) {
    CloseHandle(handle);
    return;
  }

  this->p.reset(new Impl {
    .Handle = handle,
    .Mapping = mapping,
    .Header = reinterpret_cast<Header*>(mapping),
    .Pixels = &reinterpret_cast<Pixel*>(mapping)[sizeof(Header)],
    .IsFeeder = true});
}

Writer::~Writer() {
}

Reader::Reader() {
  const auto path = fmt::format("{}/{:x}", SHM_PREFIX, SHM_VERSION);
  HANDLE handle;
  handle = CreateFileMappingA(
    INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, path.c_str());
  if (!handle) {
    return;
  }
  void* mapping = MapViewOfFile(handle, FILE_MAP_READ, 0, 0, SHM_SIZE);
  if (!mapping) {
    CloseHandle(handle);
    return;
  }

  this->p.reset(new Impl {
    .Handle = handle,
    .Mapping = mapping,
    .Header = reinterpret_cast<Header*>(mapping),
    .Pixels = &reinterpret_cast<Pixel*>(mapping)[sizeof(Header)],
    .IsFeeder = false});
}

Reader::~Reader() {
}

Reader::operator bool() const {
  return p && (p->Header->Flags & Flags::FEEDER_ATTACHED);
}

Writer::operator bool() const {
  return (bool)p;
}

Snapshot Reader::MaybeGet() const {
  if (!(*this)) {
    return {};
  }

  const auto snapshot = p->Latest;

  if (snapshot) {
    if (snapshot.GetHeader()->SequenceNumber == p->Header->SequenceNumber) {
      return snapshot;
    }
  }

  std::vector<std::byte> buffer(SHM_SIZE);
  memcpy(reinterpret_cast<void*>(buffer.data()), p->Mapping, SHM_SIZE);
  p->Latest = Snapshot(std::move(buffer));

  return p->Latest;
}

void Writer::Update(const Header& _header, const std::vector<Pixel>& pixels) {
  Header header(_header);
  if (!p) {
    throw std::logic_error("Attempted to update invalid SHM");
  }

  if (pixels.size() != header.ImageWidth * header.ImageHeight) {
    throw std::logic_error("Pixel array size does not match header");
  }

  header.Flags |= Flags::FEEDER_ATTACHED;
  header.SequenceNumber = p->Header->SequenceNumber + 1;

  std::vector<std::byte> bytes(
    sizeof(Header) + (pixels.size() * sizeof(Pixel)));
  memcpy(reinterpret_cast<void*>(bytes.data()), &header, sizeof(Header));
  memcpy(
    reinterpret_cast<void*>(&bytes.data()[sizeof(Header)]),
    pixels.data(),
    pixels.size() * sizeof(Pixel));
  memcpy(p->Mapping, bytes.data(), bytes.size());
}

}// namespace OpenKneeboard::SHM
