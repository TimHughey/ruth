#!/usr/bin/env zsh

extra=${HOME}/devel/ruth/extra

pushd ${extra}

json2msgpack -i cmds/ping.json | ncat ${1}.ruth 49151 | msgpack2json -p

popd
