--TEST--
F2: collections are iterable but remain non-objects that do not implement Traversable
--FILE--
<?php
$c = vec[int]{1, 2, 3};
var_dump(is_iterable($c));
var_dump($c instanceof Traversable);
var_dump($c instanceof Iterator);
var_dump($c instanceof IteratorAggregate);
var_dump(is_object($c));

foreach ([vec[int]{1}, set[int]{1}, tuple[int]{1}] as $k) {
    var_dump(is_iterable($k), $k instanceof Traversable);
}
echo "ok\n";
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
ok
