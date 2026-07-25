--TEST--
Collection intrinsic properties: ->count and ->isEmpty on vec/set/tuple
--FILE--
<?php
$v  = vec[int]{1, 2, 3};
$e  = vec[int]{};
$s  = set[int]{1, 1, 2};
$se = set[int]{};
$t  = tuple[int, string]{1, "a"};
$t1 = tuple[int]{1};

var_dump($v->count,  $v->isEmpty);
var_dump($e->count,  $e->isEmpty);
var_dump($s->count,  $s->isEmpty);   // set dedups: 2
var_dump($se->count, $se->isEmpty);
var_dump($t->count,  $t->isEmpty);
var_dump($t1->count, $t1->isEmpty);  // tuple arity >= 1, never empty

// frozen invariant: isEmpty === (count === 0)
foreach ([$v, $e, $s, $se, $t, $t1] as $c) {
    if ($c->isEmpty !== ($c->count === 0)) {
        echo "INVARIANT VIOLATED\n";
    }
}
echo "ok\n";
?>
--EXPECT--
int(3)
bool(false)
int(0)
bool(true)
int(2)
bool(false)
int(0)
bool(true)
int(2)
bool(false)
int(1)
bool(false)
ok
