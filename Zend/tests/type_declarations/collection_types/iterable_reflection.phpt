--TEST--
F2: Reflection of the iterable type is unchanged (ReflectionNamedType, builtin)
--FILE--
<?php
function f(iterable $x): iterable { return $x; }
$rf = new ReflectionFunction('f');
$pt = $rf->getParameters()[0]->getType();
var_dump(get_class($pt));
var_dump($pt->getName());
var_dump((string) $pt);
var_dump($pt->isBuiltin());
var_dump($pt->allowsNull());
var_dump((string) $rf->getReturnType());

function g(?iterable $x): void {}
$np = (new ReflectionFunction('g'))->getParameters()[0]->getType();
var_dump((string) $np);
var_dump($np->allowsNull());

// Contrast: a collection type still reflects as ReflectionCollectionType.
function h(vec[] $x): void {}
$ct = (new ReflectionFunction('h'))->getParameters()[0]->getType();
var_dump(get_class($ct));
var_dump((string) $ct);
echo "ok\n";
?>
--EXPECT--
string(19) "ReflectionNamedType"
string(8) "iterable"
string(8) "iterable"
bool(true)
bool(false)
string(8) "iterable"
string(9) "?iterable"
bool(true)
string(24) "ReflectionCollectionType"
string(5) "vec[]"
ok
