#!/bin/bash -

main() {
    # This is expanded by git archive
    GITARCHVERSION=$Format:%nGITHASH=%h%nGITTIME="%cd"%nGITDECO="%d"$

    git_describe

    if [ "$1" != '' ]
    then
	if [ "$VER" != '' ]||[ ! -s "$1" ]
	then {
	    echo "#define VERSION          \"$VER\""
	    echo "#define VERSION_MAJOR    $MAJOR"
	    echo "#define VERSION_MINOR    $MINOR"
	    echo "#define VERSION_BUILD    $BUILD"
	    echo "#define VERSION_HASH    \"$HASH\""
	    [ "$GITHASH" != "" ] && echo "#define GITHASH $GITHASH"
	    [ "$GITTIME" != "" ] && echo "#define GITTIME $GITTIME"
	    [ "$GITDECO" != "" ] && echo "#define GITDECO $GITDECO"
	    } > "$1"
	fi
    fi
    :
}


git_describe() {
    # Get more detail than just git describe --tags
    # tag + commits to branch from upstream + local commits + local mods.

    VER='' ; MAJOR=0 ; MINOR=0 ; BUILD=0 ; HASH='*'

    # What's upstream -- "--verify -q" is NOT silent despite the docs, sigh
    UPSTREAM=`git rev-parse --verify -q @{u} 2>/dev/null`

    [ "$UPSTREAM" = "" ] &&
	UPSTREAM=`git rev-parse --verify -q master@{u} 2>/dev/null`

    [ "$UPSTREAM" = "" ] &&
	UPSTREAM=`git rev-parse --verify -q main@{u} 2>/dev/null`

    [ "$UPSTREAM" = "" ] && UPSTREAM=HEAD

    # Find the merge base between this and the upstream.
    ORIGIN=`git merge-base $UPSTREAM HEAD`

    # Find the most recent tag.
    TAG=`git describe --tags --match='mcc-[0-9]*.[0-9]*' --abbrev=0 $ORIGIN`
    [ "$TAG" = "" ] && return

    MAJOR=`echo $TAG | sed "s/^mcc-\([0-9]*\).*/\1/"`
    MINOR=`echo $TAG | sed "s/^mcc-[0-9]*\.\([0-9]*\)/\1/"`

    # Commits between the tag and the merge base and from there to HEAD
    BUILD=`git rev-list HEAD ${TAG:+^$TAG} . | wc -l`
    DIFF=`git rev-list $ORIGIN..HEAD . | wc -l`
    BUILD=$((BUILD-DIFF))

    # Number of modified files.
    MOD=`git status --porc . | grep "^ M" | wc -l`
    DIFF=$((DIFF)) ; MOD=$((MOD))

    # Commentary.
    echo Origin ver: $MAJOR.$MINOR.$BUILD, Local commits: $DIFF, Local edits: $MOD 1>&2

    # Start of the hash
    HASH="`git rev-list HEAD -n 1 . | cut -c 1-8`"
    [ "$MOD" -ne 0 ] && HASH="$HASH/$MOD"

    VER="$MAJOR.$MINOR.$BUILD.$DIFF"
}

main "$@"
