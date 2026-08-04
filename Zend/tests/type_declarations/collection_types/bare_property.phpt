--TEST--
Bare collection-kind types: typed properties
--FILE--
<?php
class Box {
    public vec[] $items;
    public ?set[] $tags = null;
}

$b = new Box();
$b->items = vec[int]{1, 2, 3};
echo $b->items->count, "\n";        // 3
$b->tags = set[int]{7, 8};
echo $b->tags->count, "\n";         // 2
$b->tags = null;                    // nullable
var_dump($b->tags);

try { $b->items = set[int]{1}; }    // wrong kind
catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

echo $b->items->count, "\n";        // unchanged: 3
?>
--EXPECT--
3
2
NULL
Cannot assign set[int] to property Box::$items of type vec[]
3
