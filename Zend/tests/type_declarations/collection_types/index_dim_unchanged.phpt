--TEST--
Indexed access for arrays, strings and ArrayAccess objects is unchanged by the collection branch
--FILE--
<?php
// arrays: numeric-string coercion, negative/absent -> warning+null, writes work
$a = [10, 20, 30];
var_dump($a[1], $a["1"]);          // 20 20 (arrays DO coerce "1")
$a[3] = 40; $a[] = 50;
var_dump($a[3], $a[4]);            // 40 50
var_dump(isset($a[1]), isset($a[9]));  // true false

// strings: char access, negative from end
$s = "abc";
var_dump($s[0], $s[-1]);          // "a" "c"
var_dump(isset($s[1]), isset($s[9]));  // true false

// ArrayAccess object: still routed through the object handler
$o = new ArrayObject([1, 2, 3]);
var_dump($o[1]);                  // 2
$o[1] = 99;
var_dump($o[1]);                  // 99

// generator foreach still works (Traversable path untouched)
function g() { yield 1; yield 2; }
$sum = 0; foreach (g() as $x) { $sum += $x; }
var_dump($sum);                  // 3
?>
--EXPECT--
int(20)
int(20)
int(40)
int(50)
bool(true)
bool(false)
string(1) "a"
string(1) "c"
bool(true)
bool(false)
int(2)
int(99)
int(3)
