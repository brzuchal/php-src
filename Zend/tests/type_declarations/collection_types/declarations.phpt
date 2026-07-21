--TEST--
collection types: declaration positions and stringification
--FILE--
<?php
function f(vec[int] $x): vec[string] {}
class C {
    public vec[int] $prop;
    public function m(?vec[int] $a): ?vec[string] {}
    public static function s(vec[vec[int]] $n): void {}
}
$rf = new ReflectionFunction('f');
echo $rf->getParameters()[0]->getType(), "\n";
echo $rf->getReturnType(), "\n";
echo (new ReflectionProperty('C', 'prop'))->getType(), "\n";
$rm = new ReflectionMethod('C', 'm');
echo $rm->getParameters()[0]->getType(), "\n";
echo $rm->getReturnType(), "\n";
echo (new ReflectionMethod('C', 's'))->getParameters()[0]->getType(), "\n";
?>
--EXPECT--
vec[int]
vec[string]
vec[int]
?vec[int]
?vec[string]
vec[vec[int]]
