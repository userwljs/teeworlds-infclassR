# Setup Git repo configuration

This will config the merge drivers for gettext files

```shell
git config merge.merge-po-files.driver "bash ./scripts/merge-po-files.sh %A %O %B %P"
git config merge.ours.driver true
```
