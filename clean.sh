#!/bin/bash
# -*- coding: utf-8, tab-width: 2 -*-
SELFPATH="$(readlink -m "$0"/..)"


function main () {
  cd "$SELFPATH" || return $?
  local LITTER=()
  readarray -t LITTER < <(git status --short | sed -nre 's~^\?\? ~~p')
  local RM_RV=0
  if [ -n "${LITTER[*]}" ]; then
    rm -rv "${LITTER[@]}"
    RM_RV=$?
  else
    echo -en '(no litter found)\t'
  fi
  ls --all --classify --color=always .
  return $RM_RV
}













main "$@"; exit $?
