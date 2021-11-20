#!/usr/bin/env zsh

ruth_devel=${HOME}/devel/ruth

if [[ ! -d ${ruth_devel} ]]; then
  echo "Sorry, ${ruth_devel} doesn't exist."
  exit 1
fi

# establish the minimal env for idf.py
idf_path=${HOME}/${ruth_devel}
if [[ -d ${idf_path} ]]; then
  export IDF_PATH=${idf_path}
fi

# use pushd to cd to ruth devel and save the cwd
pushd -q ${ruth_devel}

# use the ESP-IDF script to finish creating the environment
# necessary to run idf.py
source ./esp-idf/export.sh &>! /tmp/idf-export.log

# upon failure, output the captured log and exit
if [[ ! $? ]]; then
  cat /tmp/idf-export.log
  rm -f /tmp/idf-export.log &> /dev/null
  popd
  exit $?
fi

# clean up after ourselves
rm -f /tmp/idf-export.log &> /dev/null

# recreate binder_0.mp, if needed

pushd -q extra/embed

binder_json=binder_0.json
binder_mp=binder_0.mp

if [[ binder_0.json -nt binder_0.mp ]]; then
  echo "creating $binder_mp ..."
  json2msgpack -i $binder_json -o $binder_mp
fi

popd

# ok, all set to invoke idf.py

# use ccache
export IDF_CCACHE_ENABLE=1

case $argv[1] in
  build)
    idf.py build | grep -v "flash"

    if [[ $? -eq 0 ]]; then
      echo "Build complete."
    else
      echo "Build failed."
    fi
    ;;

  tag-update)
    idf.py fullclean build
    ;;

  clean-build)
    idf.py clean build
    ;;

  clean)
    idf.py clean
    ;;

  flash)
    idf.py build flash monitor
    ;;

  erase)
    idf.py erase_flash build
    sleep 2
    idf.py flash monitor
    ;;

  monitor)
    idf.py monitor
    ;;

  menuconfig)
    idf.py menuconfig
    ;;

  fullclean)
    idf.py fullclean
    ;;

  size*)
    idf.py $argv[1] | egrep -v "idf_size|ninja|Running|Executing|Adding|complete|idf.py|python"
    ;;

  help)
    echo "available commands:"
    echo "build tag-update clean-build clean flash erase monitor menuconfig fullclean size"
    ;;

  *)
    idf.py build | grep -v "flash"
    idf.py flash monitor
    ;;
esac

# return to the initial cwd
popd -q
