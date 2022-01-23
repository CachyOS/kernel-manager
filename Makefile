.PHONY: pretty_format clean Changelog.md

##########################################################################
# configuration
##########################################################################

# find GNU sed to use `-i` parameter
SED:=$(shell command -v gsed || which sed)


##########################################################################
# source files
##########################################################################

# the list of sources in the src folder
SRCS=$(shell find src -type f | sort)


##########################################################################
# documentation of the Makefile's targets
##########################################################################

# main target
all:
	@echo "Changelog.md - generate ChangeLog file"
	@echo "pretty_format - beautify code"


##########################################################################
# Code format
##########################################################################

# call the Clang-Format on all source files
pretty_format:
	./scripts/format_file.sh

##########################################################################
# ChangeLog
##########################################################################

# Create a ChangeLog based on the git log using the GitHub Changelog Generator
# (<https://github.com/github-changelog-generator/github-changelog-generator>).

# variable to control the diffs between the last released version and the current repository state
NEXT_VERSION ?= "unreleased"

Changelog.md:
	github_changelog_generator -o Changelog.md --user cachyos --project kernel-manager --simple-list --release-url https://github.com/cachyos/kernel-manager/releases/tag/%s --future-release $(NEXT_VERSION)
	$(SED) -i 's|https://github.com/cachyos/kernel-manager/releases/tag/HEAD|https://github.com/cachyos/kernel-manager/tree/HEAD|' Changelog.md
	$(SED) -i '2i All notable changes to this project will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/).' Changelog.md
