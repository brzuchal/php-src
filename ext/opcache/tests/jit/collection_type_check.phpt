--TEST--
JIT TYPE_CHECK: collection === / !== null|false|true, array metadata intact
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.file_update_protection=0
opcache.jit_buffer_size=64M
opcache.jit=tracing
--EXTENSIONS--
opcache
--FILE--
<?php
/* Hot loops so the tracing JIT compiles the TYPE_CHECK. The open-world
 * MAY_BE_COLLECTION flag lives in a free bit that inference/JIT mask off, so
 * array packed/hash metadata must stay untouched while a collection "!== null"
 * is right under JIT. See implementation-notes/a5-type-check-collision.md. */

function check_collection(): int {
    $c = vec[int]{1, 2, 3};
    $bad = 0;
    for ($i = 0; $i < 200000; $i++) {
        if ($c === null)  $bad++;
        if ($c === false) $bad++;
        if ($c === true)  $bad++;
        if (!($c !== null))  $bad++;
        if (!($c !== false)) $bad++;
        if (!($c !== true))  $bad++;
    }
    return $bad;
}

function check_array(): string {
    $packed = [1, 2, 3];
    $hash   = ['a' => 1, 'b' => 2];
    $r = '';
    for ($i = 0; $i < 200000; $i++) {
        $r = ($packed !== null ? 'p1' : 'p0')
           . ($hash !== null ? 'h1' : 'h0')
           . ($packed === null ? 'x' : 'n')
           . (is_array($packed) ? 'A' : 'a')
           . (is_scalar($packed) ? 'S' : 's');
    }
    return $r . '|' . var_export($packed === [1, 2, 3], true) . ',' . var_export($hash === ['a' => 1, 'b' => 2], true);
}

printf("collection bad checks: %d\n", check_collection());
printf("array metadata: %s\n", check_array());
?>
--EXPECT--
collection bad checks: 0
array metadata: p1h1nAs|true,true
