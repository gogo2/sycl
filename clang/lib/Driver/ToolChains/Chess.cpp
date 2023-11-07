//===--- Chess.cpp - Chess Tool and ToolChain Implementation ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Chess.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;
using namespace llvm::sys;

///////////////////////////////////////////////////////////////////////////////
////                            Chess Installation Detector
///////////////////////////////////////////////////////////////////////////////

ChessInstallationDetector::ChessInstallationDetector(
    const Driver &D, const llvm::Triple &HostTriple,
    const llvm::opt::ArgList &Args)
    : D(D) {
    // This is the Xilinx wrapper for the real chesscc, this resides inside of
    // Cardano's bin directory. The real chesscc resides in Cardano's
    // Vitis/VERSION/aietools/bin/ directory
    if (llvm::ErrorOr<std::string> xchesscc = findProgramByName("xchesscc")) {
      SmallString<256> xchessccAbsolutePath;
      fs::real_path(*xchesscc, xchessccAbsolutePath);
      /// xchessccAbsolutePath will be equal to something like .../Vitis/2021.2/aietools/bin/xchesscc

      BinaryPath = xchessccAbsolutePath.str().str();

      StringRef xchessccDir = path::parent_path(xchessccAbsolutePath);

      if (path::filename(xchessccDir) == "bin")
        BinPath = xchessccDir.str();

      IsValid = true;
    }
}

///////////////////////////////////////////////////////////////////////////////
////                            Chess Linker
///////////////////////////////////////////////////////////////////////////////

void SYCL::LinkerChess::ConstructJob(Compilation &C, const JobAction &JA,
                                     const InputInfo &Output,
                                     const InputInfoList &Inputs,
                                     const ArgList &Args,
                                     const char *LinkingOutput) const {
  constructSYCLChessCommand(C, JA, Output, Inputs, Args);
}

void SYCL::LinkerChess::constructSYCLChessCommand(
    Compilation &C, const JobAction &JA, const InputInfo &Output,
    const InputInfoList &Inputs, const llvm::opt::ArgList &Args) const {
  const auto &TC =
    static_cast<const toolchains::ChessToolChain &>(getToolChain());
  ArgStringList CmdArgs;

  // This command is invoking the script at sycl/tools/sycl-chess/bin/sycl-chess

  // Script Arg $1, directory of cardano bin (where xchesscc resides)
  CmdArgs.push_back(Args.MakeArgString(TC.ChessInstallation.getBinPath()));

  // Script Arg $2, directory of the Clang driver, where the sycl-chesscc script
  // opt binary and llvm-linker binary should be contained among other things
  CmdArgs.push_back(Args.MakeArgString(C.getDriver().Dir));

  // Script Arg $3, the original source file name minus the file extension
  // (.h/.cpp etc)
  SmallString<256> SrcName =
    llvm::sys::path::filename(Inputs[0].getBaseInput());
  llvm::sys::path::replace_extension(SrcName, "");
  CmdArgs.push_back(Args.MakeArgString(SrcName));

  // Script Arg $4, input file name, distinct from Arg $3 as this is the .o
  // (it's actually a .bc file in disguise at the moment) input file with a
  // mangled temporary name
  CmdArgs.push_back(Args.MakeArgString(Inputs[0].getFilename()));

  // Script Arg $5, temporary directory path, used to dump a lot of intermediate
  // files that no one needs to know about unless they're debugging
  SmallString<256> TmpDir;
  llvm::sys::path::system_temp_directory(true, TmpDir);
  CmdArgs.push_back(Args.MakeArgString(TmpDir));

  // Script Arg $6, the name of the final output ELF binary file after
  // compilation and linking is complete
  CmdArgs.push_back(Output.getFilename());

  // Path to sycl-chess script
  SmallString<128> ExecPath(C.getDriver().Dir);
  path::append(ExecPath, "sycl-chess");
  const char *Exec = C.getArgs().MakeArgString(ExecPath);

  // Generate our command to sycl-chess using the arguments we've made
  // Note: Inputs that the shell script doesn't use should be ignored
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Exec, CmdArgs, Inputs));
}

///////////////////////////////////////////////////////////////////////////////
////                            Chess Toolchain
///////////////////////////////////////////////////////////////////////////////

ChessToolChain::ChessToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ToolChain &HostTC, const ArgList &Args)
    : ToolChain(D, Triple, Args), HostTC(HostTC),
      ChessInstallation(D, HostTC.getTriple(), Args) {
  if (ChessInstallation.isValid())
    getProgramPaths().push_back(ChessInstallation.getBinPath().str());

  // Lookup binaries into the driver directory, this is used to
  // discover the clang-offload-bundler executable.
  getProgramPaths().push_back(getDriver().Dir);
}

void ChessToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind DeviceOffloadingKind) const {
  HostTC.addClangTargetOptions(DriverArgs, CC1Args, DeviceOffloadingKind);

  assert(DeviceOffloadingKind == Action::OFK_SYCL &&
         "Only SYCL offloading kinds are supported");

  CC1Args.push_back("-fsycl-is-device");
}

Tool *ChessToolChain::buildLinker() const {
  assert(getTriple().isXilinxAIE());
  return new tools::SYCL::LinkerChess(*this);
}

void ChessToolChain::addClangWarningOptions(ArgStringList &CC1Args) const {
  HostTC.addClangWarningOptions(CC1Args);
}

ToolChain::CXXStdlibType
ChessToolChain::GetCXXStdlibType(const ArgList &Args) const {
  return HostTC.GetCXXStdlibType(Args);
}

void ChessToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                               ArgStringList &CC1Args) const {
  HostTC.AddClangSystemIncludeArgs(DriverArgs, CC1Args);
}

void ChessToolChain::AddClangCXXStdlibIncludeArgs(const ArgList &Args,
                                                  ArgStringList &CC1Args) const
{
  HostTC.AddClangCXXStdlibIncludeArgs(Args, CC1Args);
}
