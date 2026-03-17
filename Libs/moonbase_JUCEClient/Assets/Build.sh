#!/bin/sh

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=Mac;;
    CYGWIN*)    machine=Cygwin;;
    MINGW*)     machine=Win;;
    *)          machine="UNKNOWN:${unameOut}"
esac

currentos=${machine}
echo "Current OS: $currentos"

BASEDIR="$(realpath "$(dirname "$0")")"
cd "$BASEDIR"

if [ $currentos = Mac ]; then
    binaryBuilder="$BASEDIR/binaryBuilder"
elif [ $currentos = Win ]; then
    binaryBuilder="$BASEDIR/binaryBuilder.exe"
else
    echo "Unsupported OS: $currentos"
    exit 1
fi
echo "Binary Builder: $binaryBuilder"

"$binaryBuilder" "$BASEDIR/Source" "$BASEDIR" "MoonbaseBinary" || exit $?

binaryIncludes="$BASEDIR/BinaryIncludes.cpp"
if [ -f "$binaryIncludes" ]; then
    rm "$binaryIncludes"
fi
touch "$binaryIncludes"

for file in ./*.cpp; do
    if [ "$(realpath "$file")" != "$binaryIncludes" ]; then
        echo "#include \"${file:2}\"" >> "$binaryIncludes"
    fi
done

exit 0