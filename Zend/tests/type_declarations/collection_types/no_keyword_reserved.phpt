--TEST--
collection types: the head name is not a reserved keyword
--FILE--
<?php
class Vec { public const C = 1; }
function vec(int $n): int { return $n; }
const vec = 7;
echo vec(5), "\n", Vec::C, "\n", vec, "\n";
function f(vec[int] $x): void {}
echo (new ReflectionFunction('f'))->getParameters()[0]->getType(), "\n";
?>
--EXPECT--
5
1
7
vec[int]
