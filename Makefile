SHELL = /bin/sh
UNAME = $(shell uname)
MOD_NAME := Options Menu - Theme Selector
MOD_CREATOR := DefKorns
MOD_CATEGORY := User Interface

LAST_TAG_COMMIT = $(shell git rev-list --tags --max-count=1)
LAST_TAG = $(shell git describe --tags $(LAST_TAG_COMMIT) )
TAG_PREFIX = "v"
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
GIT_COMMIT := $(shell echo "`git rev-parse --short HEAD``git diff-index --quiet HEAD -- || echo '-dirty'`")
GIT_DIRTY      = $(shell git diff --shortstat 2> /dev/null | tail -n1 )
RSYNC = $(shell rsync -a mod/etc/options_menu/ temp/ --links --delete)
MOD_FILENAME   = $(shell basename `pwd`)
DEV_DIR=~/Documents/gitlab/$(MOD_FILENAME)
ifneq (,$(wildcard $(DEV_DIR)/.*))
	# LANGUAGE=$(DEV_DIR)/modules/language_pack/language
	# LOCALIZATION=$(DEV_DIR)/modules/language_pack/localization
	# PATCHES=$(DEV_DIR)/modules/patches
else
	DEV_DIR=~/Documents/_projects/hmods/$(MOD_FILENAME)
	# LANGUAGE=$(DEV_DIR)/modules/language_pack/language
	# LOCALIZATION=$(DEV_DIR)/modules/language_pack/localization
	# PATCHES=$(DEV_DIR)/modules/patches
endif

OUT=$(DEV_DIR)/out

all: hmod tar zip customlang localization patches
deploy: customlang localization patches upload

hmod: clean
	mkdir -p out/ temp/
	rsync -a mod/ temp/ --links --delete

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
	@echo $(NEXT_PATCH_VERSION) > VERSION
	
tar:
	mkdir -p out/ temp/
	$(RSYNC)
	cd temp/; tar -czf $(OUT)/$(MOD_FILENAME)-$(MOD_VER).tar.gz *
	rm -r temp/
	@echo $(NEXT_PATCH_VERSION) > VERSION

zip:
	mkdir -p out/ temp/
	#$(RSYNC)
	cd temp/; zip -r $(OUT)/$(MOD_FILENAME)-$(MOD_VER).zip *
	rm -r temp/
	@echo $(NEXT_PATCH_VERSION) > VERSION

fix: hmod tar zip
	@echo $(NEXT_PATCH_VERSION) > VERSION

update: fix
	@echo $(NEXT_MINOR_VERSION) > VERSION

upgrade: update
	@echo $(NEXT_MAJOR_VERSION) > VERSION

# customlang:
# 	@if [ ! -d $(OUT)/customlangs ]; then mkdir -p $(OUT)/customlangs temp/; fi
# 	find $(LANGUAGE)/. -mindepth 1 -maxdepth 1 -type d | xargs -n 1 basename | while IFS= read -r system; do \
# 	find $(LANGUAGE)/$$system/. -mindepth 1 -maxdepth 1 -type d | xargs -n 1 basename | while IFS= read -r country; do \
# 	rsync -a $(LANGUAGE)/$$system/$$country/ temp/ --links --delete ; \
# 	cd temp/; tar -czf $(OUT)/customlangs/$$system-$$country.tar.gz *; \
# 	cd .. && rm -r temp/ ; \
# 	done \
# 	done

# localization:
# 	find $(LOCALIZATION)/. -mindepth 1 -maxdepth 1 -type d | xargs -n 1 basename | while IFS= read -r console; do \
# 	cd $(LOCALIZATION)/"$$console" ; \
# 	tar -czf $(OUT)/"$$console"-localization.tar.gz * ;\
# 	done

# patches:
# 	@if [ ! -d $(OUT) ]; then mkdir -p $(OUT); fi
# 	find $(PATCHES)/. -mindepth 1 -maxdepth 1 -type d | xargs -n 1 basename | while IFS= read -r console; do \
# 	cd $(PATCHES)/"$$console" ; \
# 	tar -czf $(OUT)/"$$console"-patches.tar.gz * ;\
# 	done

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
	# @echo "Language: $(LANGUAGE)"
	# @echo "Localization: $(LOCALIZATION)"
	# @echo "Patches: $(PATCHES)"

clean:
	-rm -rf out/ temp/

.PHONY: clean
