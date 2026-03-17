#!/bin/bash
reset
cd "$(dirname "$0")/.."

RUN_VALIDATION_BENCHMARK=0

matrixJUCE="$1"
matrixCpp="$2"
runsOnGithub="$3"

# on windows invoke vcvarsall.bat to setup the environment
if [[ -z "$runsOnGithub" ]]; then
    if [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]]; then
        eval "$(./tests/msvc-init/vcvarsall.sh x64)"
    fi
fi


CXX_DEFINES="-DMB_CATCH2_TESTING -DJUCE_MODAL_LOOPS_PERMITTED=1"

if [ $RUN_VALIDATION_BENCHMARK -eq 1 ]; then
    CXX_DEFINES="${CXX_DEFINES} -DRUN_VALIDATION_BENCHMARK"
fi

# Conditionally add defines
if [ "${matrixJUCE}" == "JUCE7" ]; then
    CMAKE_JUCE="-DJUCE7=ON"
else
    CMAKE_JUCE=""
fi

if [ "${matrixCpp}" == "20" ]; then
    CMAKE_CPP="-DCPP20=ON"
else
    CMAKE_CPP=""
fi

CMAKE_ARGS="-B Builds -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=sccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=sccache \
    -DCMAKE_CXX_FLAGS=\"${CXX_DEFINES}\" \
    ${CMAKE_JUCE} ${CMAKE_CPP}
"

export numCpusCores=${NPROC:-$(nproc || sysctl -n hw.ncpu || echo 4)}


eval cmake ${CMAKE_ARGS} . || exit 1
cmake --build Builds --parallel ${numCpusCores} || exit 2

TESTCOMMAND="ctest --test-dir Builds --parallel ${numCpusCores}"
if [ $RUN_VALIDATION_BENCHMARK -eq 1 ]; then
    TESTCOMMAND="${TESTCOMMAND} -V"
fi
eval ${TESTCOMMAND} || exit 3

exit 0