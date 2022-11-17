#!/usr/bin/env zsh

build=${HOME}/devel/ruth/build
bin_file=${build}/ruth.bin

fw_path=/dar/www/wisslanding/htdocs/sally/firmware
fw_name=00.00-lightdesk-ruth.bin

pushd ${build}
sudo -u helen rsync -a ${bin_file} ${fw_path}/${fw_name}

pushd ${fw_path}
ls -al ${fw_name}

rpc=$(<<'RPC'
Sally.host_ota("dmx", latest: true)
Sally.host_ota("test-with-devs", latest: true)
RPC
)

helen rpc "${rpc}"

popd +2
