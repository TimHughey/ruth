#!/usr/bin/env zsh

ruth_devel=${HOME}/devel/ruth

if [[ ! -d ${ruth_devel} ]]; then
  echo "Sorry, ${ruth_devel} doesn't exist."
  exit 1
fi

# use pushd to cd to ruth devel and save the cwd
pushd -q ${ruth_devel}

host=$(hostname -s)
if [[ "${host}" = "jophiel" ]]; then
  git pull || exit 1
fi

# use the ESP-IDF script to finish creating the environment
# necessary to run idf.py
source ./esp-idf/export.sh &>! /tmp/idf-export.log

export IDF_CCACHE_ENABLE=1

# if it failed output the captured log and exit
if [[ ! $? ]]; then
  cat /tmp/idf-export.log
  rm -f /tmp/idf-export.log &> /dev/null
  popd
  exit $?
fi

# clean up after ourselves
rm -f /tmp/idf-export.log &> /dev/null

if [[ sdkconfig.defaults -nt sdkconfig ]]; then
  echo "sdkconfig.defaults are new, removing sdkconfig and cleaning"
  rm -f sdkconfig

  idf.py clean &> /dev/null

  if [[ ! $? ]]; then
    popd -q
    echo "idf.py clean failed, aborting"
    exit $?
  fi

fi

idf.py build

if [[ ! $? ]]; then
  popd
  exit $?
fi

fw_suffixes=(bin elf)
vsn=$(git describe)
htdocs=/dar/www/wisslanding/htdocs/helen/firmware

pushd build

for suffix in "${fw_suffixes[@]}"; do

  if [[ ! -f ruth.${suffix} ]]; then
    echo "can't find ruth.${suffix}, aborting"

    popd -q +2
    exit $?
  fi


  if [[ "${host}" != "jophiel" ]]; then
    scp ruth.${suffix} jophiel:${htdocs}

    if [[ ! $? ]]; then
      popd -q +2
      exit $?
    fi

    ssh jophiel "pushd -q $htdocs ; \
                 rm -f latest.${suffix} ; \
                 ln -s ./ruth.${suffix} latest.${suffix}; \
                 ls -l latest.${suffix}"

    if [[ ! $? ]]; then
      popd -q +2
      exit $?
    fi

  else
    cp ruth.${suffix} ${htdocs}/${vsn}-ruth.${suffix}

    if [[ ! $? ]]; then
      popd -q +2
      exit $?
    fi

    # cd into htdocs for the remainder
    pushd -q ${htdocs}

    # point the well known name latest-ruth.* to the new file
    rm -f latest.${suffix}

    if [[ ! $? ]]; then
      popd -q +2
      exit $?
    fi

    ln -s ./${vsn}-ruth.${suffix} latest.${suffix}

		ls -l latest.${suffix}
		popd -q
  fi
done

popd -q
