--TEST--
F2: iterator_apply() stays Traversable-only and rejects collections (and arrays)
--FILE--
<?php
try {
    iterator_apply(vec[int]{1}, fn() => true);
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    iterator_apply([1, 2], fn() => true);
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
echo "ok\n";
?>
--EXPECTF--
iterator_apply(): Argument #1 ($iterator) must be of type Traversable, collection given
iterator_apply(): Argument #1 ($iterator) must be of type Traversable, array given
ok
