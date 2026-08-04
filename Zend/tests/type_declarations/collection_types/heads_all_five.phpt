--TEST--
collection types: the head is recognised lexically, the kind travels in the AST
--FILE--
<?php
/* vec is implemented end to end. The type prints from the descriptor, so the
 * kind reached the compiler from the AST attribute rather than from a name
 * lookup on a head token. */
function a(vec[int] $x): void {}
function b(?vec[vec[int]] $x): void {}
function c(): vec[string] {}

foreach (['a', 'b', 'c'] as $f) {
    $r = new ReflectionFunction($f);
    echo $f, ': ', $f === 'c' ? $r->getReturnType() : $r->getParameters()[0]->getType(), "\n";
}

/* The head names remain ordinary identifiers everywhere else. */
function vec(): int { return 1; }
function map(): int { return 2; }
function set(): int { return 3; }
function tuple(): int { return 4; }
function shape(): int { return 5; }
class vec { const C = 6; }
const map = 7;
echo vec(), map(), set(), tuple(), shape(), vec::C, map, "\n";

/* Member access is never a collection head. */
class K { const vec = [8]; const map = [0, 9]; }
$o = new stdClass; $o->set = [10];
echo K::vec[0], K::map[1], $o->set[0], "\n";
?>
--EXPECT--
a: vec[int]
b: ?vec[vec[int]]
c: vec[string]
1234567
8910
