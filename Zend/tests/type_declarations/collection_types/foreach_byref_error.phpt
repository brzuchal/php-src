--TEST--
Collection foreach: by-reference iteration throws a kind-named Error (variable and temporary sources)
--FILE--
<?php
function tryRef(callable $make): void {
    try {
        foreach ($make() as &$v) {}
        echo "NO ERROR\n";
    } catch (\Error $e) {
        echo get_class($e), ": ", $e->getMessage(), "\n";
    }
}
tryRef(fn() => vec[int]{1, 2, 3});
tryRef(fn() => set[int]{1, 2});
tryRef(fn() => tuple[int, string]{1, "a"});

// variable source is rejected the same way, and the value survives the rejected loop
$c = vec[int]{9};
try { foreach ($c as &$v) {} } catch (\Error $e) { echo $e->getMessage(), "\n"; }
echo "survived: ";
foreach ($c as $v) { echo $v; }
echo "\n";
?>
--EXPECT--
Error: Cannot iterate over vec by reference
Error: Cannot iterate over set by reference
Error: Cannot iterate over tuple by reference
Cannot iterate over vec by reference
survived: 9
