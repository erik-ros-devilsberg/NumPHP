# Makefile fragment — concatenated into the generated Makefile by
# PHP_ADD_MAKEFILE_FRAGMENT() in config.m4 at ./configure time.
#
# Purpose: override phpize's auto-generated `clean` and `distclean`
# rules. Upstream phpize emits recursive `find . -name '*.so' | xargs
# rm -f` (and the same for `.lo`, `.o`, `.dep`, `.la`, `.a`, and `.libs`
# directories), which deletes ANY matching file under the project tree
# — including bench/.venv/.../*.so when numpy's C extensions live
# under it, or anything else a contributor may have placed locally.
#
# This file replaces those rules with explicit, scoped paths. No
# recursive `find` allowed. If a new build artefact path is added, list
# it explicitly here.
#
# Make warns "overriding recipe for target 'clean'" when this fragment
# wins over Makefile.global's recipe. That warning is expected and
# intended — it is the override doing its job.

clean:
	rm -rf src/.libs modules/.libs
	rm -f src/*.lo src/*.o src/*.dep src/*.gcno src/*.gcda
	rm -f modules/numphp.so modules/numphp.la
	rm -f *.la
	rm -f a-conftest.gcno a-conftest.gcda conftest.gcno conftest.gcda
	rm -f ext/opcache/jit/ir/gen_ir_fold_hash
	rm -f ext/opcache/jit/ir/minilua
	rm -f ext/opcache/jit/ir/ir_fold_hash.h
	rm -f ext/opcache/jit/ir/ir_emit_x86.h
	rm -f ext/opcache/jit/ir/ir_emit_aarch64.h

distclean: clean
	rm -f Makefile config.cache config.log config.status Makefile.objects Makefile.fragments libtool main/php_config.h
