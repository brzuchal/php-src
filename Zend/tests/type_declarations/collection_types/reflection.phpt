--TEST--
collection types: ReflectionCollectionType
--FILE--
<?php
function f(vec[vec[string]] $x): void {}
$t = (new ReflectionFunction('f'))->getParameters()[0]->getType();
var_dump($t instanceof ReflectionCollectionType);
var_dump($t instanceof ReflectionType);
var_dump($t instanceof ReflectionNamedType, $t instanceof ReflectionUnionType);
echo $t->getCollectionName(), "\n";
echo (string) $t, "\n";
var_dump($t->allowsNull());
$inner = $t->getTypes()[0];
echo get_class($inner), " ", (string) $inner, "\n";
$leaf = $inner->getTypes()[0];
echo get_class($leaf), " ", $leaf->getName(), " ", var_export($leaf->isBuiltin(), true), "\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)
vec
vec[vec[string]]
bool(false)
ReflectionCollectionType vec[string]
ReflectionNamedType string true
