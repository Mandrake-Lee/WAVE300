#!/bin/sh

FW_MBSS_API_SUBDIR=shared_mbss_mac

parse_svn_branch() {
  parse_svn_url | sed -e 's#^'"$(parse_svn_repository_root)"'##g' | awk -F / '{ print ( $1 == "trunk" ) ? $1 : $1 "/" $2 }'
}

parse_svn_url() {
  svn info $TARGET_DIR 2>/dev/null | grep -e '^URL*' | sed -e 's#^URL: *\(.*\)#\1#g '
}

parse_svn_repository_root() {
  svn info $TARGET_DIR 2>/dev/null | grep -e '^Repository Root:*' | sed -e 's#^Repository Root: *\(.*\)#\1\/#g '
}

get_fw_api_version() {
  svn info $TARGET_DIR/wireless/$FW_API_SUBDIR | grep "^Last Changed Rev: *" | sed -e 's#^Last Changed Rev: *\(.*\)#\1#'
}

get_svn_info() {
TARGET_DIR=$1
BASE_INFO=$2

  if [ -d $TARGET_DIR/.svn ]; then 
    # Under source control => get SVN info
    SVN_VERSION="unknown"
    SVN_BRANCH="unknown"
    # if svnversion exists then set SVN_VERSION to svnversion stdout
    hash svnversion &>/dev/null && SVN_VERSION=`svnversion $TARGET_DIR/`
    # if svn exists then add to SVN_BRANCH branch info
    hash svn &>/dev/null && SVN_BRANCH=$(parse_svn_branch)
    SVN_INFO=$SVN_VERSION"."$SVN_BRANCH
  else
    # Source tarball => mark as 'exported' + base version if exists
    VERSION="exported"
    [ "$BASE_INFO" ] && SVN_INFO=$VERSION"."$BASE_INFO
  fi

  echo $SVN_INFO
}

get_fw_api_info() {
TARGET_DIR=$1
FW_API_SUBDIR=$2
BASE_INFO=$3

FW_API_INFO="unknown"

  if [ -d $TARGET_DIR/.svn ]; then 
    # Under source control => get SVN info
    # if svn exists then get FW_API_INFO branch info
    hash svn &>/dev/null && FW_API_INFO=$(get_fw_api_version)
  else
    # Source tarball => mark as 'exported' + base version if exists
    [ "$BASE_INFO" ] && FW_API_INFO=$BASE_INFO
  fi

  echo $FW_API_INFO
}

TARGET_DIR=$1
test x"$TARGET_DIR" = x""  && TARGET_DIR="."

INFO_SVN=0
INFO_FW_SBSS_API=0
INFO_FW_MBSS_API=0
test x"$2" = x"" && INFO_SVN=1 && INFO_FW_SBSS_API=1 && INFO_FW_MBSS_API=1
test x"$2" = x"svn" && INFO_SVN=1
test x"$2" = x"fw_sbss_api" && INFO_FW_SBSS_API=1
test x"$2" = x"fw_mbss_api" && INFO_FW_MBSS_API=1

# get base version from 'version' file if exists BEFORE we output (probaly to the same file)
if [ -f $TARGET_DIR/svn_info ]; then
  BASE_SVN_INFO=$(cat $TARGET_DIR/svn_info | awk '{ print $1; }')
  BASE_FW_SBSS_API_INFO=$(cat $TARGET_DIR/svn_info | awk '{ print $2; }')
  BASE_FW_MBSS_API_INFO=$(cat $TARGET_DIR/svn_info | awk '{ print $3; }')
fi

test x"$INFO_SVN" = x"1" && INFO="$INFO $(get_svn_info $TARGET_DIR $BASE_SVN_INFO)"
test x"$INFO_FW_MBSS_API" = x"1" && INFO="$INFO $(get_fw_api_info $TARGET_DIR $FW_MBSS_API_SUBDIR $BASE_FW_MBSS_API_INFO)"

echo $INFO
