#!/usr/bin/env zsh

base=${HOME}/devel/ruth
extra=${base}/extra



pushd $base

source $base/esp-idf/export.sh 1>/dev/null 2>/dev/null

# the first make ensures everything is compiled
# idf.py size 1>/dev/null || exit 1
idf.py size 1>/dev/null 2>/dev/null || exit 1

# the second make records the clean output

case $argv[1] in
  --no-log)
    tracker=/dev/stdout
    ;;
  *)
    tracker=${extra}/firmware-size/tracker.txt
    ;;
esac

echo ">>>" >> ${tracker}
git log -1 1>>${tracker}
idf.py size | grep -e Total -e DRAM -e Used -e Flash 1>>${tracker}
echo "<<<" >> ${tracker}

popd
