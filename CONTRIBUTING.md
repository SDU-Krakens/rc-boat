# Contribution Guide

This document uses the ASD-STE100 Simplified Technical English rules.

## 1. Overview

All team members work on the same repository. Each person does their work
on a separate branch. A branch keeps your changes separate from `main`
until a reviewer approves them.

## 2. Rules

1. Do not commit directly to `main`. Use a branch.
2. Pull `main` before you make a new branch.
3. Each pull request must contain one feature or one fix only.
4. Build and test your code before you make a pull request.
5. Review the pull requests of other team members.
6. If you do not understand something, ask in the team chat.

## 3. Names

### 3.1 Branch names

Use one of these prefixes:

| Prefix  | Use                | Example           |
| ------- | ------------------ | ----------------- |
| `feat/` | A new feature      | `feat/gps`        |
| `fix/`  | A fix for a defect | `fix/gps-parsing` |

Use short names. Use dashes between words. Do not use spaces.

### 3.2 Commit messages

Use the Conventional Commits format
(<https://www.conventionalcommits.org/en/v1.0.0/>):

```
<type>[(scope)][!]: <description>

[body]

[footer]
```

Use one of these types:

| Type       | Use                                                   |
| ---------- | ----------------------------------------------------- |
| `feat`     | A new feature                                         |
| `fix`      | A fix for a defect                                    |
| `docs`     | Changes to documentation only                         |
| `style`    | Changes to formatting only. The code does not change  |
| `refactor` | Changes to the code that do not add a feature or a fix |
| `perf`     | Changes that make the code faster                     |
| `test`     | New tests or changes to tests                         |
| `build`    | Changes to the build system (`Makefile`, CMake)       |
| `ci`       | Changes to the CI configuration                       |
| `chore`    | Other changes that do not change the firmware         |
| `revert`   | Removal of a previous commit                          |

The scope is optional. It is the part of the code that the commit changes,
for example `gps`, `lora` or `comm`.

Write the description in the imperative and in lower case. Do not put a
period at the end.

If the commit changes the behavior in a way that is not compatible with
the previous version, put `!` before the colon. Also add a
`BREAKING CHANGE:` footer that gives the change.

| Correct                                  | Incorrect   |
| ---------------------------------------- | ----------- |
| `feat(log): add logging for UART`        | `stuff`     |
| `fix(spi): stop crash at start`          | `changes`   |
| `docs: describe the LoRa packet format`  | `fast push` |
| `feat(comm)!: add a CRC to each frame`   | `Fixed GPS.` |

## 4. Procedure

### 4.1 Get the latest code

Before you start work, get the latest version of `main`:

```bash
git checkout main
git pull origin main
```

### 4.2 Make a branch

To make a new branch, type:

```bash
git checkout -b feat/your-feature-name
```

If the branch exists, switch to it:

```bash
git checkout feat/your-feature-name
```

### 4.3 Commit your changes

1. Make your changes.
2. Build and test the code.
3. Look at the changed files:

   ```bash
   git status
   ```

4. Add only the files that you want to commit:

   ```bash
   git add path/to/file.c
   ```

5. Commit the changes:

   ```bash
   git commit -m "feat: add logging for UART"
   ```

Commit frequently. Each commit must contain one change.

### 4.4 Push your branch

Push your branch to GitHub:

```bash
git push origin feat/your-feature-name
```

On the first push of a new branch, Git can show a command that sets the
upstream branch. Type that command.

### 4.5 Make a pull request

1. Open the repository on GitHub.
2. Click **Pull requests**, then **New pull request**.
3. Select `main` as the base branch and your branch as the compare branch.
4. Write a title. Use the same format as a commit message.
5. Write a description. Include these items:
   - The feature or the fix that you added.
   - The procedure to test it.
   - Your questions or problems, if you have them.
6. Click **Create pull request**.

### 4.6 Code review

1. Wait for a review from one or more team members.
2. Read all comments. Change the code or answer each comment.
3. Push the changes to the same branch. The pull request shows them
   automatically.

### 4.7 Merge and clean up

When a reviewer approves the pull request:

1. Click **Merge pull request** on GitHub.
2. Delete your branch. GitHub shows a button for this.
3. Switch to `main` and get the merged changes:

   ```bash
   git checkout main
   git pull origin main
   ```

## 5. Special conditions

### 5.1 Another person merged changes into `main`

Put the latest changes of `main` into your branch:

```bash
git checkout main
git pull origin main
git checkout feat/your-feature-name
git merge main
```

If Git shows a conflict, do these steps:

1. Open each file that Git shows.
2. Find the `<<<<<<<`, `=======` and `>>>>>>>` markers.
3. Keep the correct code and remove the markers.
4. Add the files and complete the merge:

   ```bash
   git add path/to/file.c
   git commit
   ```

5. Push your branch:

   ```bash
   git push origin feat/your-feature-name
   ```

If you cannot solve a conflict, ask a team member for help. Do not use
`git push --force`.

### 5.2 Your last commit has an error

If you did not push the commit, remove it and keep the changes:

```bash
git reset --soft HEAD~1
```

Then correct the changes and commit again.

If you pushed the commit, do not change it. Make a new commit with the fix.

### 5.3 You made changes on `main`

1. Make a branch. Your changes move to the new branch:

   ```bash
   git checkout -b feat/your-feature-name
   ```

2. Commit your changes on the new branch (refer to 4.3).
3. Push the branch:

   ```bash
   git push origin feat/your-feature-name
   ```

4. Reset your local `main` to the GitHub version:

   ```bash
   git checkout main
   git reset --hard origin/main
   ```

> **CAUTION:** `git reset --hard` removes all changes on `main` that are not
> on a different branch. Do step 4 only after step 3 is complete.

## 6. Help

- For help with Git commands, ask in the team chat or refer to
  <https://git-scm.com/docs>.
- If you cannot solve a merge conflict, ask a team member to work on it
  with you.
- If you broke something, stop and ask for help. Git keeps the full
  history, and the team can repair the problem.

## 7. Quick reference

```bash
# Start new work
git checkout main
git pull origin main
git checkout -b feat/your-feature-name

# Save your work
git status
git add path/to/file.c
git commit -m "feat: your change"
git push origin feat/your-feature-name

# Put the latest main into your branch
git checkout main
git pull origin main
git checkout feat/your-feature-name
git merge main

# Switch to a different branch
git checkout branch-name

# Show the current branch
git branch

# Show the changed files
git status
```
