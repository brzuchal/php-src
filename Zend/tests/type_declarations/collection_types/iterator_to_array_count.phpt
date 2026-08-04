--TEST--
F2: iterator_to_array() and iterator_count() accept native collections; their internal Reflection stays Traversable|array
--FILE--
<?php
$v = vec[int]{10, 20, 30};
echo json_encode(iterator_to_array($v)), "\n";
echo json_encode(iterator_to_array($v, false)), "\n";
echo json_encode(iterator_to_array($v, true)), "\n";
echo implode(",", array_keys(iterator_to_array($v))), "\n";
echo json_encode(iterator_to_array(set[int]{3, 1, 3, 2})), "\n";
echo json_encode(iterator_to_array(tuple[int, string]{7, "z"})), "\n";
echo json_encode(iterator_to_array(vec[int]{})), "\n";
echo iterator_count($v), "\n";
echo iterator_count(vec[int]{}), "\n";
echo iterator_count(set[int]{3, 1, 3, 2}), "\n";
echo iterator_count(tuple[int, string]{7, "z"}), "\n";

// Decision 1: the arginfo now carries _ZEND_TYPE_ITERABLE_BIT so collections are
// accepted, but the internal Reflection projection is unchanged -- a
// ReflectionUnionType rendering "Traversable|array" with members [Traversable, array].
foreach (['iterator_to_array', 'iterator_count'] as $fn) {
    $t = (new ReflectionFunction($fn))->getParameters()[0]->getType();
    printf("%s: %s %s [%s]\n", $fn, get_class($t), (string) $t,
        implode(',', array_map(fn($m) => (string) $m, $t->getTypes())));
}
echo "ok\n";
?>
--EXPECT--
[10,20,30]
[10,20,30]
[10,20,30]
0,1,2
[3,1,2]
[7,"z"]
[]
3
0
3
2
iterator_to_array: ReflectionUnionType Traversable|array [Traversable,array]
iterator_count: ReflectionUnionType Traversable|array [Traversable,array]
ok
