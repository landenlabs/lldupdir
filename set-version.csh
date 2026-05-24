#!/bin/csh -f

# Verify two arguments are provided
if ( $#argv < 2 ) then
    echo "Usage: $0 <tag_version> <commit_message>"
    echo "Example: $0 v1.2.3 'Update copyright notice'"
    exit 1
endif

set tag = "$1"
set msg = "$2"


# commit before tagging so the tag points to the new changes.
git add .
git commit -m "$msg"

# Check if Existing Tag (Local and Remote)
# We check if the tag exists locally first.
git rev-parse "$tag" >& /dev/null
if ( $status == 0 ) then
    echo "Tag $tag exists. Resetting to current commit..."
    git tag -d "$tag"
endif

# Create the new annotated tag on the NEW commit
git tag -a "$tag" -m "$msg"

# Push changes and the tag
git push origin 

# Second, push the tag (force allows overwriting the old tag on GitHub)
git push origin "$tag" --force

echo "Completed commit with tag $tag"
git tag -l --format="%(refname:short)%09%(creatordate:short)%09%(contents:subject)"
