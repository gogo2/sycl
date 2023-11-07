# -*- Python -*-

import os
import platform
import re
import subprocess
import tempfile
import getpass
from distutils.spawn import find_executable

import lit.formats
import lit.util

from lit.llvm import llvm_config

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = 'SYCL'

# testFormat: The test format to use to interpret tests.
#
# For now we require '&&' between commands, until they get globally killed and
# the test runner updated.
config.test_format = lit.formats.ShTest()

# suffixes: A list of file extensions to treat as test files.
dump_only_tests = bool(lit_config.params.get('SYCL_LIB_DUMPS_ONLY', False))
if dump_only_tests:
    config.suffixes = ['.dump'] # Only run dump testing
else:
    config.suffixes = ['.c', '.cpp', '.dump', '.test'] #add .spv. Currently not clear what to do with those

# feature tests are considered not so lightweight, so, they are excluded by default
config.excludes = ['Inputs', 'feature-tests', 'disabled', '_x', '.Xil', '.run', 'span']

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.sycl_obj_root, 'test')

# Propagate some variables from the host environment.
llvm_config.with_system_environment(['PATH', 'OCL_ICD_FILENAMES', 'SYCL_DEVICE_ALLOWLIST', 'SYCL_CONFIG_FILE_NAME', 'SYCL_PI_TRACE', 'XILINX_XRT'])

aie=lit_config.params.get('AIE', "off")
vitis=lit_config.params.get('VITIS', "off")

config.substitutions.append(('%python', '"%s"' % (sys.executable)))

# Propagate extra environment variables
if config.extra_environment:
    lit_config.note("Extra environment variables")
    for env_pair in config.extra_environment.split(','):
        [var,val]=env_pair.split("=")
        if val:
           llvm_config.with_environment(var,val)
           lit_config.note("\t"+var+"="+val)
        else:
           lit_config.note("\tUnset "+var)
           llvm_config.with_environment(var,"")

config.environment['SYCL_VXX_PRINT_CMD'] = 'True'
config.environment['SYCL_VXX_SERIALIZE_VITIS_COMP'] = 'True'
config.environment['XRT_PCIE_HW_EMU_FORCE_SHUTDOWN'] = 'True'
config.environment['SYCL_VXX_TEST_MODE'] = 'True'

# Configure LD_LIBRARY_PATH or corresponding os-specific alternatives
# Add 'libcxx' feature to filter out all SYCL abi tests when SYCL runtime
# is built with llvm libcxx. This feature is added for Linux only since MSVC
# CL compiler doesn't support to use llvm libcxx instead of MSVC STL.
if platform.system() == "Linux":
    config.available_features.add('linux')
    if config.sycl_use_libcxx == "ON":
        config.available_features.add('libcxx')
    llvm_config.with_system_environment('LD_LIBRARY_PATH')
    llvm_config.with_environment('LD_LIBRARY_PATH', config.sycl_libs_dir, append_path=True)

elif platform.system() == "Windows":
    config.available_features.add('windows')
    llvm_config.with_system_environment('LIB')
    llvm_config.with_environment('LIB', config.sycl_libs_dir, append_path=True)

elif platform.system() == "Darwin":
    # FIXME: surely there is a more elegant way to instantiate the Xcode directories.
    llvm_config.with_system_environment('CPATH')
    llvm_config.with_environment('CPATH', "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1", append_path=True)
    llvm_config.with_environment('CPATH', "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/", append_path=True)
    llvm_config.with_environment('DYLD_LIBRARY_PATH', config.sycl_libs_dir)

llvm_config.with_environment('PATH', config.sycl_tools_dir, append_path=True)

config.substitutions.append( ('%threads_lib', config.sycl_threads_lib) )
config.substitutions.append( ('%sycl_libs_dir',  config.sycl_libs_dir ) )
config.substitutions.append( ('%sycl_include',  config.sycl_include ) )
config.substitutions.append( ('%sycl_source_dir', config.sycl_source_dir) )
config.substitutions.append( ('%opencl_libs_dir',  config.opencl_libs_dir) )
config.substitutions.append( ('%level_zero_include_dir',  config.level_zero_include_dir) )
config.substitutions.append( ('%opencl_include_dir',  config.opencl_include_dir) )
config.substitutions.append( ('%cuda_toolkit_include',  config.cuda_toolkit_include) )
config.substitutions.append( ('%sycl_tools_src_dir',  config.sycl_tools_src_dir ) )
config.substitutions.append( ('%llvm_build_lib_dir',  config.llvm_build_lib_dir ) )
config.substitutions.append( ('%llvm_build_bin_dir',  config.llvm_build_bin_dir ) )
config.substitutions.append( ('%clang_offload_bundler', f'{config.llvm_build_bin_dir}clang-offload-bundler') )

llvm_symbolizer = os.path.join(config.llvm_build_bin_dir, 'llvm-symbolizer')
llvm_config.with_environment('LLVM_SYMBOLIZER_PATH', llvm_symbolizer)

config.substitutions.append( ('%fsycl-host-only', '-std=c++17 -Xclang -fsycl-is-host -isystem %s -isystem %s -isystem %s -isystem %s' % (config.sycl_include, config.level_zero_include_dir, config.opencl_include_dir, config.sycl_include + '/sycl/') ) )
config.substitutions.append( ('%sycl_lib', ' -lsycl6' if platform.system() == "Windows" else '-lsycl') )

llvm_config.add_tool_substitutions(['llvm-spirv'], [config.sycl_tools_dir])

backend=lit_config.params.get('SYCL_BE', "PI_OPENCL")
lit_config.note("Backend (SYCL_BE): {}".format(backend))
config.substitutions.append( ('%sycl_be', backend) )

triple=lit_config.params.get('SYCL_TRIPLE', 'spir64-unknown-unknown')
lit_config.note("Triple: {}".format(triple))
config.substitutions.append( ('%sycl_triple',  triple ) )

additional_flags = config.sycl_clang_extra_flags.split(' ')

if config.cuda_be == "ON":
    config.available_features.add('cuda_be')

if config.hip_be == "ON":
    config.available_features.add('hip_be')

if config.esimd_emulator_be == "ON":
    config.available_features.add('esimd_emulator_be')

if triple == 'nvptx64-nvidia-cuda':
    llvm_config.with_system_environment('CUDA_PATH')
    config.available_features.add('cuda')

if triple == 'amdgcn-amd-amdhsa':
    llvm_config.with_system_environment('ROCM_PATH')
    config.available_features.add('hip_amd')
    # For AMD the specific GPU has to be specified with --offload-arch
    if not any([f.startswith('--offload-arch') for f in additional_flags]):
        # If the offload arch wasn't specified in SYCL_CLANG_EXTRA_FLAGS,
        # hardcode it to gfx906, this is fine because only compiler tests
        additional_flags += ['-Xsycl-target-backend=amdgcn-amd-amdhsa',
                            '--offload-arch=gfx906']

# Dump-only tests do not have clang available
if not dump_only_tests:
    llvm_config.use_clang(additional_flags=additional_flags)

filter=lit_config.params.get('SYCL_PLUGIN', "opencl")

lit_config.note("Filter: {}".format(filter))

acc_run_substitute=f"env SYCL_DEVICE_FILTER={filter} "
extra_compile_flags=""
if vitis != "off" and vitis != "cpu":
    # Clean up the named semaphore in case the previous test did not clean up properly.
    # If someone tries to run multiple tests on the same machine this could cause issues.
    os.system("rm -rf /dev/shm/sem.sycl_vxx.py")
    # xrt doesn't deal well with multiple executables using it concurrently (at the time of writing).
    # The details are at https://xilinx.github.io/XRT/master/html/multiprocess.html
    # so we wrap every use of XRT inside an file lock.
    xrt_lock = f"{tempfile.gettempdir()}/xrt-{getpass.getuser()}.lock"
    acc_run_substitute+= "flock --exclusive " + xrt_lock + " "
    if os.path.exists(xrt_lock):
        os.remove(xrt_lock)
    acc_run_substitute="env --unset=XCL_EMULATION_MODE " + acc_run_substitute
    # hw_emu is very slow so it has a higher timeout.
    acc_run_substitute += "unshare --pid --map-current-user --kill-child "
    if "hw_emu" not in triple:
        acc_run_substitute+= "timeout 300 env "
    else:
        acc_run_substitute+= "timeout 600 env "
if vitis == "cpu":
    extra_compile_flags=" -fsyntax-only "
    # This will print the command instead of executing it
    # Since nothing is being fully compiled nothing should ne executed
    acc_run_substitute="echo "

config.substitutions.append( ('%ACC_RUN_PLACEHOLDER', acc_run_substitute) )
config.substitutions.append( ('%EXTRA_COMPILE_FLAGS', extra_compile_flags) )

timeout = 600
if vitis == "off":
    config.excludes += ['vitis']
else:
    lit_config.note(f"vitis mode: {vitis}")
    if vitis == "cpu":
        config.available_features.add("vitis_cpu")
    required_env = ['HOME', 'USER', 'XILINX_XRT', 'XILINX_PLATFORM', 'EMCONFIG_PATH', 'LIBRARY_PATH', 'XILINX_VITIS']
    has_error=False
    config.available_features.add("vitis")
    feat_list = ",".join(config.available_features)
    lit_config.note(f"Features: {feat_list}")
    pkg_opencv4 = subprocess.run(["pkg-config", "--libs", "--cflags", "opencv4"], stdout=subprocess.PIPE)
    has_opencv4 = not pkg_opencv4.returncode
    lit_config.note("has opencv4: {}".format(has_opencv4))
    if has_opencv4:
        config.available_features.add("opencv4")
        config.substitutions.append( ('%opencv4_flags', pkg_opencv4.stdout.decode('utf-8')[:-1]) )
    for env in required_env:
        if env not in os.environ:
            lit_config.note("missing environnement variable: {}".format(env))
            has_error=True
    if has_error:
        lit_config.error("Can't configure tests for Vitis")
    llvm_config.with_system_environment(required_env)
    if vitis == "only":
        config.excludes += ['basic_tests', 'extentions', 'online_compiler', 'plugins']
    # run_if_* defaults to a simple echo to print the comand instead of running it.
    # it will be replaced by and empty string to actually run the command.
    run_if_hw="echo"
    run_if_hw_emu="echo"
    run_if_sw_emu="echo"
    if "_hw-" in triple:
        timeout = 32400 # 9h
        run_if_hw=""
    if "_hw_emu" in triple:
        timeout = 3600 # 1h
        run_if_hw_emu=""
    if "_sw_emu" in triple:
        timeout = 1200 # 20min
        run_if_sw_emu=""
    run_if_not_cpu="echo"
    if vitis != "cpu":
        run_if_not_cpu = ""
    config.substitutions.append( ('%run_if_hw', run_if_hw) )
    config.substitutions.append( ('%run_if_hw_emu', run_if_hw_emu) )
    config.substitutions.append( ('%run_if_sw_emu', run_if_sw_emu) )
    config.substitutions.append( ('%run_if_not_cpu', run_if_not_cpu) )

if aie == "off":
    config.excludes += ['aie', 'acap']
else:
    config.excludes += ['abi', 'basic_tests', 'CMakeLists.txt', 'extensions', 'gdb', 'invoke_simd', 'matrix', 'optional_kernel_features', 'scheduler', 'type_traits', 'vitis', 'check_device_code', 'esimd', 'fpga_tests', 'kernel_param', 'multi_ptr', 'regression', 'tools', 'Unit', 'warnings']
    if "aie" in aie:
        make_sh_path = os.environ["AIE_MAKE_SH"]
        lit_config.note(f"using aie make.sh: {make_sh_path}")
        config.available_features.add("aie")
        config.excludes += ['acap']
        config.substitutions.append( ('%aie_clang', f"{make_sh_path} {config.clang}++"))
    else:
        make_sh_path = os.environ["ACAP_MAKE_SH"]
        lit_config.note(f"using acap make.sh: {make_sh_path}")
        config.available_features.add("acap")
        config.excludes += ['aie']
        config.substitutions.append( ('%acap_clang', f"{make_sh_path} {config.clang}++"))
        add_acap_result="echo"
        if "ACAP_COLLECT_TEST_BIN_PATH" in os.environ:
            ACAP_COLLECT_TEST_BIN_PATH = os.environ["ACAP_COLLECT_TEST_BIN_PATH"]
            lit_config.note(f"collecting results into: {ACAP_COLLECT_TEST_BIN_PATH}")
            subprocess.run(["rm", "-rf", f"{ACAP_COLLECT_TEST_BIN_PATH}"])
            subprocess.run(["mkdir", f"{ACAP_COLLECT_TEST_BIN_PATH}"])
            add_acap_result = f"cp --target-directory={ACAP_COLLECT_TEST_BIN_PATH} "
            config.substitutions.append( ('%add_acap_result', add_acap_result))
    if "AIE_RUN_ON_DEVICE_SH" in os.environ:
        run_on_device = os.environ["AIE_RUN_ON_DEVICE_SH"]
        config.substitutions.append( ('%run_on_device', run_on_device))
        lit_config.note(f"using aie run_on_device.sh: {run_on_device}")
    if aie == "aie_no_device":
        config.available_features.add("no_device")
        config.substitutions.append( ('%if_run_on_device', "true "))
    else:
        config.substitutions.append( ('%if_run_on_device', " "))
    llvm_config.with_environment('ACAP_MAKE_IN_PARALLEL', '1')
    required_env = ['HOME', 'USER', 'XILINXD_LICENSE_FILE', 'LM_LICENSE_FILE', 'RDI_INTERNAL_ALLOW_PARTIAL_DATA', 'AIE_ROOT', 'CHESSROOT']
    has_error=False
    for env in required_env:
        if env not in os.environ:
            lit_config.note("missing environnement variable: {}".format(env))
            has_error=True
    if has_error:
        lit_config.error("Can't configure tests for AIE or ACAP")
    llvm_config.with_system_environment(required_env)

# Set timeout for test = 10 mins
try:
    import psutil
    lit_config.maxIndividualTestTime = timeout
except ImportError:
    pass
