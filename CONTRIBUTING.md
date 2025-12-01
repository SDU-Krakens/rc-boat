# Git Collaboration Guide

(https://github.com/SDU-Krakens/rc-boat/blob/main/CONTRIBUTION.pdf)[PDF version for GitHub Desktop]

## Overview
We're all working on the same project, but each person works on their own feature branch. This keeps everyone's work separate until it's ready to merge.

## The Basic Workflow

### 1. Get the Latest Code
Before starting work, always get the latest version:
```bash
git checkout main
git pull origin main
```

### 2. Create Your Feature Branch (if it doesn't exist)
Create a new branch for what you're working on:
```bash
git checkout -b featyour-feature-name
```

**Branch naming:**
- Use `feature/` or `feat/` prefix for new features (e.g., `feat/gps`)
- Use `fix/` prefix for bug fixes (e.g., `fix/gps-parsing`)
- Keep names short and descriptive
- Use dashes, not spaces

### 3. Work on Your Code
- Make your changes
- Test that everything works
- Commit often with clear messages

**Committing your changes:**
```bash
git add .
git commit -m "Add login form with validation"
```

**Good commit messages:**
- ✅ "Add logging for UART"
- ✅ "Fix issue where SPI crashes"
- ❌ "stuff"
- ❌ "changes"

### 4. Push Your Branch
Push your branch to GitHub:
```bash
git push origin feat/your-feature-name
```

If it's your first push on this branch, Git will tell you to set upstream - just copy and run the command it suggests.

### 5. Create a Pull Request (PR)
1. Go to the GitHub repository
2. Click "Pull requests" → "New pull request"
3. Select your branch to merge into `main`
4. Write a clear title and description of what you did
5. Click "Create pull request"

**In your PR description, include:**
- What feature/fix you added
- How to test it
- Any questions or concerns

### 6. Code Review
- Wait for at least one team member to review your code
- Address any feedback or questions
- Once approved, the PR can be merged

### 7. Merge and Clean Up
After your PR is approved:
1. Click "Merge pull request" on GitHub
2. Delete your feature branch (GitHub will offer this option)
3. Switch back to main locally and pull the changes:
```bash
git checkout main
git pull origin main
```

## Common Scenarios

### Someone else merged their code - what do I do?
Update your feature branch with the latest changes:
```bash
git checkout main
git pull origin main
git checkout feat/your-feature-name
git merge main
```

If there are conflicts, Git will tell you which files. Open them, fix the conflicts (look for `<<<<<<<` markers), then:
```bash
git add .
git commit -m "Merge main into feature branch"
git push origin feat/your-feature-name
```

### I made a mistake in my commit
If you haven't pushed yet:
```bash
git reset --soft HEAD~1  # Undo last commit but keep changes
```

If you already pushed, just make a new commit with the fix.

### I accidentally worked on main instead of a feature branch
Create the branch now and move your changes:
```bash
git checkout -b feat/your-feature-name
git push origin feat/your-feature-name
```

Then reset main:
```bash
git checkout main
git reset --hard origin/main
```

## Rules to Remember

1. **Never commit directly to main** - always use a feature branch
2. **Always pull before creating a new branch** - start with the latest code
3. **Keep your PRs focused** - one feature or fix per PR
4. **Test your code** before creating a PR
5. **Review others' PRs** - we all learn from each other
6. **Ask questions** - if you're confused, others probably are too

## Getting Help

### Stuck on a Git command?
Ask in the team chat or check: https://git-scm.com/docs

### Merge conflict you can't figure out?
Don't force anything - ask a teammate to pair with you.

### Broke something?
Don't panic! Git keeps history of everything. We can always fix it.

## Quick Reference

```bash
# Start working on something new
git checkout main
git pull origin main
git checkout -b feature/your-feature

# Save your work
git add .
git commit -m "Feat: Your feature description"
git push origin feat/your-feature

# Update your branch with latest main
git checkout main
git pull origin main
git checkout feat/your-feature
git merge main

# Switch between branches
git checkout branch-name

# See what branch you're on
git branch

# See what files changed
git status
```

## Questions?
If anything is unclear, ask! This is a learning process for all of us.
