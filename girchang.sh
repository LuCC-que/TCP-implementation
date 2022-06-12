#!/bin/sh

git filter-branch -f --env-filter  '
NEW_NAME="LuCC-que"
NEW_EMAIL="chen1968493689@gmail.com"
if [ "$GIT_COMMIT" = "1eaccd9e32f6e7eb8b689d524ce6b6663a74c5de" ] || [ "$GIT_COMMIT" = "b2303437a10c6e601feb8b368229a83abc254219" ]
then
    export GIT_COMMITTER_NAME="$NEW_NAME"
    export GIT_COMMITTER_EMAIL="$NEW_EMAIL"
    export GIT_AUTHOR_NAME="$NEW_NAME"
    export GIT_AUTHOR_EMAIL="$NEW_EMAIL"
fi
' --tag-name-filter cat -- --branches --tags 