#!/usr/bin/env zsh

build=${HOME}/devel/ruth/build
bin_file=${build}/ruth.bin

fw_path=/dar/www/wisslanding/htdocs/sally/firmware
fw_name=lightdesk.bin

pushd ${build}
rsync -a ${bin_file} ${fw_path}/${fw_name}

pushd ${fw_path}
ls -al ${fw_name}

rpc=$(<<'RPC'
Sally.host_ota("dmx", file_regex: ~r/lightdesk.bin/)
Sally.host_ota("test-with-devs", file_regex: ~r/lightdesk.bin/)
RPC
)

helen rpc "${rpc}"

popd +2
