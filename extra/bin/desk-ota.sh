#!/usr/bin/env zsh

build=${HOME}/devel/ruth/build
extra=${HOME}/devel/ruth/extra
bin_file=${build}/ruth.bin

fw_path=/dar/www/wisslanding/htdocs/desk/fw

pushd ${build}
rsync -a ${bin_file} ${fw_path}

popd
pushd ${extra}

json2msgpack -i cmds/ota.json | ncat ${1}.ruth 49151 | msgpack2json -p

popd
