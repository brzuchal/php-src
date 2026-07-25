--TEST--
Collection intrinsic properties: dynamic property names use the same path
--FILE--
<?php
$v = vec[int]{1, 2, 3};
$e = vec[int]{};

foreach (['count', 'isEmpty'] as $name) {
    var_dump($v->$name);
    var_dump($e->$name);
}

// unknown dynamic name throws, exactly like an unknown direct name
try {
    $name = 'unknown';
    $v->$name;
} catch (\Error $ex) {
    echo $ex->getMessage(), "\n";
}
?>
--EXPECT--
int(3)
int(0)
bool(false)
bool(true)
Undefined intrinsic property "unknown" on collection
