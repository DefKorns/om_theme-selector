git config core.eol lf
git config core.autocrlf input
git rm --cached -rf .
git diff --cached --name-only -z | xargs -n 50 -0 git add -f
git ls-files -z | xargs -0 rm
git checkout .
