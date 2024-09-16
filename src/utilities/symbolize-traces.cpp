/*
 * OpenKneeboard C API Header
 *
 * Copyright (C) 2022-2023 Fred Emmott <fred@fredemmott.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * ----------------------------------------------------------------
 * ----- THE ABOVE LICENSE ONLY APPLIES TO THIS SPECIFIC FILE -----
 * ----------------------------------------------------------------
 *
 * The majority of OpenKneeboard is under a different license; check specific
 * files for more information.
 */
#include <Windows.h>

#include <combaseapi.h>

#include <wil/com.h>
#include <wil/resource.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>
#include <ranges>
#include <regex>
#include <string>
#include <unordered_map>

#include <Dia2.h>
#include <diacreate.h>

#pragma comment(lib, "diaguids")

std::wstring utf8_to_wide(std::string_view utf8) {
  const auto wideCharCount = MultiByteToWideChar(
    CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring ret;
  ret.resize(wideCharCount, L'\0');
  MultiByteToWideChar(
    CP_UTF8,
    0,
    utf8.data(),
    static_cast<int>(utf8.size()),
    ret.data(),
    wideCharCount);
  return ret;
}

std::string wide_to_utf8(std::wstring_view wide) {
  const auto byteCount = WideCharToMultiByte(
    CP_UTF8, 0, wide.data(), wide.size(), nullptr, 0, nullptr, nullptr);
  std::string ret;
  ret.resize(byteCount, '\0');
  WideCharToMultiByte(
    CP_UTF8,
    0,
    wide.data(),
    wide.size(),
    ret.data(),
    byteCount,
    nullptr,
    nullptr);
  return ret;
}

static void usage(int argc, char** argv) {
  std::println(
    stderr, "Usage: {} [--pdb-path FOLDER_PATH] CRASH_TEXT_FILE", argv[0]);
}

int main(int argc, char** argv) {
  std::filesystem::path pdbDirectory;
  std::filesystem::path logFile;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg {argv[i]};
    if (arg == "--help" || arg == "/?") {
      usage(argc, argv);
      return EXIT_FAILURE;
    }
    if (arg == "--pdb-path") {
      if (i + 1 >= argc) {
        usage(argc, argv);
        return EXIT_FAILURE;
      }
      pdbDirectory = {argv[++i]};
      continue;
    }

    if (!logFile.empty()) {
      usage(argc, argv);
      return EXIT_FAILURE;
    }

    logFile = {arg};
  }

  if (logFile.empty()) {
    usage(argc, argv);
    return EXIT_FAILURE;
  }

  if (!(std::filesystem::exists(logFile)
        && std::filesystem::is_regular_file(logFile))) {
    std::println(
      stderr, "`{}` does not exist or is not a file", logFile.string());
    return EXIT_FAILURE;
  }

  auto comInit = wil::CoInitializeEx();

  wil::com_ptr<IDiaDataSource> diaSource;
  {
    const auto result = NoRegCoCreate(
      L"msdia140.dll", CLSID_DiaSource, IID_PPV_ARGS(diaSource.put()));
    if (!diaSource) {
      std::println(
        stderr,
        "Failed to load the DIA SDK (part of Visual Studio): {:#010x}",
        static_cast<uint32_t>(result));
      return EXIT_FAILURE;
    }
  }

  const auto hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  {
    // Handle intentionally not closed per MSDN docs
    DWORD mode = 0;
    if (GetConsoleMode(hStdout, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
      SetConsoleMode(hStdout, mode);
    }
  }

  wil::com_ptr<IDiaSession> diaSession;
  wil::com_ptr<IDiaSymbol> diaGlobal;

  struct function_t {
    DWORD mSection {};
    DWORD mOffset {};
    DWORD mRelative {};
  };

  struct module_t {
    std::unordered_map<std::string, function_t> mFunctions;
  };

  std::unordered_map<std::string, module_t> seenModules;

  // Can be a module or function offset
  //
  // 0> OpenKneeboardApp+0x85B5
  // 5> OpenKneeboardApp!VSDesignerDllMain+0x5A394
  const std::regex entry_regex {"^(\\d+)> (\\w+)(!(\\w+))?\\+0x([A-Z0-9]+)$"};
  std::smatch entry_match;

  if (pdbDirectory.empty()) {
    wchar_t buf[MAX_PATH];
    auto charCount = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    pdbDirectory
      = std::filesystem::path {std::wstring_view {buf, charCount - 1}}
          .parent_path();
  }

  std::ifstream f(logFile, std::ios::binary);
  std::string line;
  while (std::getline(f, line)) {
    if (!std::regex_match(line, entry_match, entry_regex)) {
      std::println("{}", line);
      continue;
    }

    const auto counter = entry_match[1];
    const auto module = entry_match[2].str();
    const auto function = entry_match[4].str();
    const auto raw_offset = std::stoi(entry_match[5], nullptr, 16);

    bool loadedPdb = false;
    if (!seenModules.contains(module)) {
      seenModules[module] = {};
      const auto pdbPath = (pdbDirectory / module).replace_extension(".pdb");
      if (std::filesystem::exists(pdbPath)) {
        wil::verify_hresult(
          diaSource->loadDataFromPdb(pdbPath.wstring().c_str()));
        loadedPdb = true;
      }
    }
    if (loadedPdb) {
      wil::verify_hresult(diaSource->openSession(diaSession.put()));
      wil::verify_hresult(diaSession->get_globalScope(diaGlobal.put()));

      {
        wil::com_ptr<IDiaEnumDebugStreams> streams;
        wil::verify_hresult(diaSession->getEnumDebugStreams(streams.put()));
        wil::com_ptr<IDiaEnumDebugStreamData> stream;
        LONG fetched = 0;
        while (SUCCEEDED(streams->Next(1, stream.put(), 0))) {
          if (!stream) {
            continue;
          }
        }
      }

      wil::com_ptr<IDiaEnumSymbols> symbols;
      wil::verify_hresult(
        diaGlobal->findChildren(SymTagFunction, nullptr, 0, symbols.put()));
      wil::com_ptr<IDiaSymbol> symbol;
      ULONG fetched = 0;
      while (SUCCEEDED(symbols->Next(1, symbol.put(), &fetched))) {
        if (!symbol) {
          break;
        }
        function_t fun;
        wil::verify_hresult(symbol->get_addressSection(&fun.mSection));
        wil::verify_hresult(symbol->get_addressOffset(&fun.mOffset));
        wil::verify_hresult(symbol->get_relativeVirtualAddress(&fun.mRelative));

        wil::unique_bstr wname;
        symbol->get_name(wname.put());
        const auto name = wide_to_utf8(wname.get());

        seenModules[module].mFunctions[name] = std::move(fun);
      }
    }

    std::optional<function_t> frame;

    const auto& functions = seenModules.at(module).mFunctions;
    if (function.empty()) {
      auto fn_view = functions | std::views::values;
      auto it = std::ranges::max_element(
        fn_view, {}, [raw_offset](const auto& it) -> int64_t {
          if (it.mRelative > raw_offset) {
            return -1;
          }
          return it.mRelative;
        });
      if (it != fn_view.cend()) {
        const auto& base = *it;
        const auto offset = raw_offset - base.mRelative;
        frame = {
          .mSection = base.mSection,
          .mOffset = (raw_offset - base.mRelative) + base.mOffset,
        };
      }
    } else {
      const auto it = functions.find(function);
      if (it != functions.end()) {
        frame = {
          .mSection = it->second.mSection,
          .mOffset = it->second.mOffset + raw_offset,
        };
      }
    }

    if (frame) {
      wil::com_ptr<IDiaSymbol> symbol;

      wil::verify_hresult(diaSession->findSymbolByAddr(
        frame->mSection, frame->mOffset, SymTagFunction, symbol.put()));
      DWORD functionBaseOffset {};
      symbol->get_addressOffset(&functionBaseOffset);

      wil::unique_bstr wname;
      symbol->get_name(wname.put());
      auto symbolName = wide_to_utf8(wname.get());

      wil::com_ptr<IDiaEnumLineNumbers> linesEnum;
      wil::verify_hresult(diaSession->findLinesByAddr(
        frame->mSection, frame->mOffset, 1, linesEnum.put()));

      wil::com_ptr<IDiaLineNumber> diaLine;
      linesEnum->Item(0, diaLine.put());

      wil::com_ptr<IDiaSourceFile> diaFile;
      wil::verify_hresult(diaLine->get_sourceFile(diaFile.put()));
      wil::unique_bstr fileNameW;
      diaFile->get_fileName(fileNameW.put());
      const auto fileName = wide_to_utf8(fileNameW.get());

      DWORD firstLine {};
      DWORD lastLine {};
      diaLine->get_lineNumber(&firstLine);
      diaLine->get_lineNumberEnd(&lastLine);

      std::println(
        "{}> \x1b[33m{}({})\x1b[0m: {}!\x1b[32m{}\x1b[0m+0x{:0X}",
        counter.str(),
        fileName,
        firstLine,
        module,
        symbolName,
        frame->mOffset - functionBaseOffset);
      continue;
    }
    std::println("{}", line);
  }

  return 0;
}