--TEST--
F2: by-reference foreach over a collection is still rejected (unchanged from F1)
--FILE--
<?php
$c = vec[int]{1, 2, 3};
var_dump(is_iterable($c));
try {
    foreach ($c as &$v) {}
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    foreach (set[int]{1} as &$v) {}
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
echo "ok\n";
?>
--EXPECT--
bool(true)
Cannot iterate over vec by reference
Cannot iterate over set by reference
ok
