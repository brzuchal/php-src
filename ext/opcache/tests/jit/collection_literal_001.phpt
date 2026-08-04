--TEST--
JIT collection literal: an unsupported opcode falls back to the VM handler
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.jit_buffer_size=16M
opcache.jit=tracing
opcache.file_update_protection=0
--EXTENSIONS--
opcache
zend_test
--FILE--
<?php

/* The JIT does not compile ZEND_CONSTRUCT_COLLECTION; it must fall back to the
 * VM handler. Run the site hot enough to be traced, inside a loop the JIT will
 * want to compile, and check that the values are still correct. */

function build(int $n) {
    $v = vec[int]{$n, $n + 1, $n + 2};
    return zend_test_vec_get($v, 2);
}

$sum = 0;
for ($i = 0; $i < 2000; $i++) {
    $sum += build($i % 7);
}
var_dump($sum);

function nested(int $n) {
    return vec[vec[int]]{ vec[int]{$n}, vec[int]{} };
}

$ids = [];
for ($i = 0; $i < 2000; $i++) {
    $ids[zend_test_vec_type_id(nested($i))] = true;
}
var_dump(count($ids) === 1);

function bad(int $n) {
    try {
        $v = vec[int]{$n, 'x'};
    } catch (TypeError $e) {
        return 1;
    }
    return 0;
}

$errors = 0;
for ($i = 0; $i < 1000; $i++) {
    $errors += bad($i);
}
var_dump($errors);

?>
--EXPECT--
int(9995)
bool(true)
int(1000)
