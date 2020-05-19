#!/usr/bin/env zsh

base=${HOME}/devel/ruth
extra=${base}/extra

tracker=${extra}/firmware-size/tracker.txt

pushd $base

source $base/esp-idf/export.sh

# the first make ensures everything is compiled
# idf.py size 1>/dev/null || exit 1
idf.py size || exit 1 

# the second make records the clean output
echo ">>>" >> ${tracker}
git log -1 1>>${tracker}
idf.py size 1>>${tracker}
echo "<<<" >> ${tracker}

popd
