--TEST--
vec/tuple indexed read: owned value copy; object identity kept, nested collection/array immutable
--FILE--
<?php
// object element: a read-mode fetch returns the SAME object (identity), not a clone.
// A direct `$v[0]->n = 1` is a writable dim fetch and is rejected (see
// index_write_rejected.phpt); the supported form is a two-step borrow, which mutates
// the shared instance and is visible through every later read of the vec.
$o = new stdClass; $o->n = 1;
$v = vec[stdClass]{$o};
var_dump($v[0] === $o);              // true: same instance
$x = $v[0];                          // read-mode fetch: borrow the shared object
$x->n = 42;                          // a normal object write on the shared instance
var_dump($o->n === 42);              // visible through the original handle
var_dump($v[0]->n === 42);           // ...and through a fresh read of the vec
var_dump($v->count === 1);           // the vec itself is structurally untouched

// nested collection element: reading returns the nested value; it stays immutable
$inner = vec[int]{1, 2, 3};
$outer = tuple[vec[int], int]{$inner, 9};
$got = $outer[0];
var_dump($got[0], $got[1], $got[2]);  // 1 2 3
var_dump($got->count === 3);

// array element is a value: the returned copy is COW, the stored element unaffected
$vs = vec[array]{[1, 2]};
$a = $vs[0];
$a[] = 3;                              // mutate the local copy
var_dump($vs[0]);                      // still [1, 2]

// chained indexing through a nested collection read
var_dump($outer[0][2]);               // 3
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
int(1)
int(2)
int(3)
bool(true)
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
int(3)
