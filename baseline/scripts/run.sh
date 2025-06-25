# from inside baseline directory
# NOT FROM ROOT DIRECTORY PLS    

if [ "$(basename "$PWD")" != "baseline2" ]; then
  echo "This script must be run from inside the 'baseline' directory!"
  exit 1
fi

rm -rf build
mkdir build
cd build

cmake .. \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++

make 
