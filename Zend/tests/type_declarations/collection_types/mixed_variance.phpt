--TEST--
collection types: a collection may narrow a mixed prototype, but stays invariant otherwise
--FILE--
<?php
class P {
    public function ret(): mixed {}
    public function arg(vec[int] $a): void {}
}
class Q extends P {
    public function ret(): vec[int] {}   // narrowing mixed: covariant, allowed
    public function arg(mixed $a): void {}  // widening to mixed: contravariant, allowed
}
echo "narrowing a mixed return to vec[int] accepted\n";
echo "widening a vec[int] parameter to mixed accepted\n";
?>
--EXPECT--
narrowing a mixed return to vec[int] accepted
widening a vec[int] parameter to mixed accepted
