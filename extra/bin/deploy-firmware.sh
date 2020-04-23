#!/usr/bin/env zsh

host=$(hostname -s)

function run_cmd {
    "$@"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "error with $1" >&2
				exit 1
    fi
    return $rc
}

function sudo_cmd {
    sudo -u ruth "$@"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "error with $1" >&2
				exit 1
    fi
    return $rc
}

base=${HOME}/devel/ruth
build=${base}/build
fw_suffixes=(bin elf)
vsn=$(git describe)
htdocs=/dar/www/wisslanding/htdocs/helen/firmware

. ${base}/esp-idf/export.sh 1> /dev/null 2> /dev/null

pushd -q $base

if [[ "${host}" = "jophiel" ]]; then
  git pull || exit 1
fi

if [[ -x ${IDF_PATH}/add-path.sh ]]; then
	${IDF_PATH}/add-path.sh 1> /dev/null
fi

run_cmd idf.py build

# echo "deploying ruth.{bin,elf} to $htdocs"
for suffix in "${fw_suffixes[@]}"; do
  src=${build}/ruth.${suffix}
  dest=${vsn}-ruth.${suffix}
  latest=latest.${suffix}

  if [[ "${host}" != "jophiel" ]]; then
    run_cmd scp ${src} jophiel:${htdocs}/${dest}
    ssh jophiel "pushd -q ${htdocs} ; rm -f ${latest}"
    ssh jophiel "pushd -q ${htdocs} ; ln -s ./${dest} ${latest}"
  else
    pushd -q $htdocs
    run_cmd cp $src $dest || exit 1

    # point the well known name latest-ruth.* to the new file
    sudo_cmd rm -f $latest
    sudo_cmd ln -s ./${dest} $latest

		ls -l $latest
		popd -q
  fi
done

popd -q
