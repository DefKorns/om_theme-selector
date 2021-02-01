SHELL = /bin/sh
UNAME = $(shell uname)
MOD_NAME := Options Menu - Theme Selector
MOD_CREATOR := DefKorns
MOD_CATEGORY := User Interface

LAST_TAG_COMMIT = $(shell git rev-list --tags --max-count=1)
LAST_TAG = $(shell git describe --tags $(LAST_TAG_COMMIT) )
TAG_PREFIX = "v"
CURRENT_BRANCH = $(shell git rev-parse --abbrev-ref HEAD)
GIT_REMOTES    = $(shell git remote | xargs echo )
GIT_DIRTY      = $(shell git diff --shortstat 2> /dev/null | tail -n1 )
GET_VER    = $(shell  git describe --tags $(LAST_TAG_COMMIT) | sed "s/^$(TAG_PREFIX)//")
#MOD_VER  = $(shell [ -f VERSION ] && head VERSION || echo "0.0.1")
MOD_VER  = $(shell [ -f VERSION ] && head VERSION || echo $(GET_VER))
MAJOR      = $(shell echo $(MOD_VER) | sed "s/^\([0-9]*\).*/\1/")
MINOR      = $(shell echo $(MOD_VER) | sed "s/[0-9]*\.\([0-9]*\).*/\1/")
PATCH      = $(shell echo $(MOD_VER) | sed "s/[0-9]*\.[0-9]*\.\([0-9]*\).*/\1/")

# total number of commits
BUILD      = $(shell git log --oneline | wc -l | sed -e "s/[ \t]*//g")
NEXT_MAJOR_VERSION = $(shell expr $(MAJOR) + 1).0.0
NEXT_MINOR_VERSION = $(MAJOR).$(shell expr $(MINOR) + 1).0-b$(BUILD)
NEXT_PATCH_VERSION = $(MAJOR).$(MINOR).$(shell expr $(PATCH) + 1)-b$(BUILD)

MOD_URL=`git config --get remote.origin.url`
GIT_COMMIT := $(shell echo "`git rev-parse --short HEAD``git diff-index --quiet HEAD --`")
GIT_DIRTY      = $(shell git diff --shortstat 2> /dev/null | tail -n1 )
RSYNC = $(shell rsync -a mod/etc/options_menu/ temp/ --links --delete)
MOD_FILENAME   = $(shell basename `pwd`)
DEV_DIR=/d/om_theme-selectorv2
# DEV_DIR=~/Documents/gitlab/$(MOD_FILENAME)
# ifneq (,$(wildcard $(DEV_DIR)/.*))
# else
# 	DEV_DIR=~/Documents/_projects/hmods/$(MOD_FILENAME)
# endif


OUT=$(DEV_DIR)/out

all: hmod tar zip
deploy: customlang localization patches upload

hmod: clean
	mkdir -p out/ temp/
	rsync -a --exclude-from=exclude-file.txt mod/ temp/ --links --delete

	printf "%s\n" \
	"---" \
	"Name: $(MOD_NAME)" \
	"Creator: $(MOD_CREATOR)" \
	"Category: $(MOD_CATEGORY)" \
	"Version: $(MOD_VER)" \
	"Built on: $(shell date +"%A, %d %b %Y - %T")" \
	"Git commit: $(GIT_COMMIT)" \
	"---" > temp/readme.md
	
	sed 1d mod/readme.md >> temp/readme.md

	cd temp/; tar -czf $(OUT)/$(MOD_FILENAME)-$(MOD_VER).hmod *
	rm -r temp/
	
tar:
	mkdir -p out/ temp/
	$(RSYNC)
	cd temp/; tar -czf $(OUT)/$(MOD_FILENAME)-$(MOD_VER).tar.gz *
	rm -r temp/

zip:
	mkdir -p out/ temp/
	#$(RSYNC)
	cd temp/; zip -r $(OUT)/$(MOD_FILENAME)-$(MOD_VER).zip *
	rm -r temp/

fix: hmod tar zip
	@echo $(NEXT_PATCH_VERSION) > VERSION

update: fix
	@echo $(NEXT_MINOR_VERSION) > VERSION

upgrade: update
	@echo $(NEXT_MAJOR_VERSION) > VERSION

upload:
	rm -f $(OUT)/$(MOD_FILENAME).*
	rsync -e ssh --progress --exclude 'rsync*' --exclude 'src' -avzzp out/* defkorns@hakchicloud.com:/var/www/html/Hakchi_Themes/options_menu

info:
	@echo "Mod Dir: $(MOD_FILENAME)"
	@echo "Current version: $(MOD_VER)"
	@echo "Last tag: $(LAST_TAG)"
	@echo "$(shell git rev-list $(LAST_TAG).. --count) commit(s) since last tag"
	@echo "Build: $(BUILD) (total number of commits)"
	@echo "next major version: $(NEXT_MAJOR_VERSION)"
	@echo "next minor version: $(NEXT_MINOR_VERSION)"
	@echo "next patch version: $(NEXT_PATCH_VERSION)"

clean:
	rm -rf out/ temp/

up:
	$(if $(GIT_DIRTY), $(error "Unable to pull subtree(s): Dirty Git repository"))
	@for elem in $(GIT_SUBTREE_REPOS); do \
		url=`echo $$elem | cut -d '|' -f 1`; \
		repo=`basename $$url .git`; \
		path=`echo $$repo | tr '-' '/'`; \
		br=`echo $$elem | cut -d '|' -f 2`;  \
		[ "$$br" == "$$url" ] && br='master'; \
		echo -e "\n===> pulling changes into subtree '$$path' using remote '$$repo/$$br'"; \
		echo -e "     \__ fetching remote '$$repo'"; \
		git fetch $$repo; \
		echo -e "     \__ pulling changes"; \
		git subtree pull --prefix $$path --squash $${repo} $${br}; \
	done

.PHONY: clean
