./create_edit_mappings  \
-db MUTAG -method F2 -method_options threads 30

./create_edit_mappings \
-db MUTAG -method REFINE -method_options threads 30 max-swap-size 5 lower-bound-method BRANCH

