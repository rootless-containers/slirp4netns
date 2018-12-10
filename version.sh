#!/bin/sh

# Copyright (c) 2018 Anton Semjonov
# Licensed under the MIT License

#-------------------------------------------#
#                                           #
#  This script prints version information   #
#         when executed in a shell:         #
#                                           #
#             sh ./version.sh               #
#                                           #
#         For more information see:         #
#   https://github.com/ansemjo/version.sh   #
#                                           #
#-------------------------------------------#

# Ignore warnings about necessary single-quotes and literal newlines for POSIX compatability:
# shellcheck disable=SC2016,SC1004

# add 'VERSION export-subst' in .gitattributes and
# these strings will be substituted by git-archive
COMMIT='$Format:%H$'
REFS='$Format:%D$'

# constants
FALLBACK_VERSION='commit'
FALLBACK_COMMIT='unknown'
REVISION='.r'

# check if variable contains a subst value or still has the format string
hasval() { expr "$1" : '$Format' == 0 >/dev/null; }

# parse the %D reflist to get tag or branch
refparse() { REF="$1";
  tag=$(echo "$REF" | sed -ne 's/.*tag: \([^,]*\).*/\1/p'); test -n "$tag" && echo "$tag" && return 0;
  branch=$(echo "$REF" | sed -e 's/HEAD -> //' -e 's/, /\
/' | sed -ne '/^[a-z0-9._-]*$/p' | sed -n '1p'); test -n "$branch" && echo "$branch" && return 0;
  return 1; }

# git functions to return commit and version in repository
hasgit() { test -d .git; }
gitcommit() { hasgit && git describe --always --abbrev=0 --match '^$' --dirty; }
gitversion() { hasgit \
  && { V=$(git describe 2>/dev/null) && echo "$V" | sed 's/-\([0-9]*\)-g.*/'"$REVISION"'\1/'; } \
  || { C=$(git rev-list --count HEAD) && printf '0.0.0%s%s' "$REVISION" "$C"; };
}

# wrappers
version() { hasval "$REFS" && refparse "$REFS" || gitversion || echo "$FALLBACK_VERSION"; }
commit()  { hasval "$COMMIT" && echo "$COMMIT" || gitcommit || echo "$FALLBACK_COMMIT";  }
describe() { printf '%s-g%.7s\n' "$(version)" "$(commit)"; }

# ---------------------------------

case "$1" in
  version)  version ;;
  commit)   commit ;;
  describe) describe ;;
  json)
    printf '{\n  "version": "%s",\n  "commit": "%s",\n  "describe":"%s"\n}\n' "$(version)" "$(commit)" "$(describe)" ;;
  help)
    printf '%s [version|commit|describe|json]\n' "$0" ;;
  *)
    printf 'version : %s\ncommit  : %s\n' "$(version)" "$(commit)" ;;
esac
