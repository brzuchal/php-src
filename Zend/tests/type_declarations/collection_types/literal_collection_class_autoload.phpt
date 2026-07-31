--TEST--
collection literals: class-typed element validation at FINISH may autoload; safe and exception-chained
--FILE--
<?php
// Validating a class-typed slot calls zend_lookup_class(), which can run the autoloader --
// arbitrary userland -- inside FINISH_COLLECTION. The payload is a complete, unpublished
// owned TMP at that point, so an autoloader that throws, GCs, or re-enters is safe: the
// payload unwinds/scans over its initialized slots exactly like the former array path.

spl_autoload_register(function ($cls) {
    echo "  autoload($cls)\n";
    if ($cls === 'Boom') { throw new RuntimeException('autoload failed'); }
    eval("class $cls {}");
});

echo "-- autoload runs during vec[Class] validation (value mismatches, lookup still happens) --\n";
try { $v = vec[Lazy1]{ new stdClass }; }
catch (\TypeError $e) { echo "  ", $e->getMessage(), "\n"; }

echo "-- autoload runs for a positional tuple member (member 1) --\n";
try { $t = tuple[int, Lazy2]{ 1, new stdClass }; }
catch (\TypeError $e) { echo "  ", $e->getMessage(), "\n"; }

echo "-- autoloader throws: the element TypeError carries it as previous (nothing leaks) --\n";
try { $v = vec[Boom]{ new stdClass }; }
catch (\TypeError $e) {
    echo "  ", get_class($e), " <- previous ", get_class($e->getPrevious()), ": ", $e->getPrevious()->getMessage(), "\n";
}
try { $t = tuple[int, Boom]{ 1, new stdClass }; }
catch (\TypeError $e) { echo "  tuple previous: ", $e->getPrevious()->getMessage(), "\n"; }

echo "-- a matching, already-loaded class validates with no autoload --\n";
class Point {}
$p = new Point;
$v = vec[Point]{ $p, new Point };
var_dump($v->count, $v[0] === $p);
$t = tuple[Point, int]{ $p, 5 };
var_dump($t[0] === $p, $t[1]);
?>
--EXPECT--
-- autoload runs during vec[Class] validation (value mismatches, lookup still happens) --
  autoload(Lazy1)
  Element 0 of vec[Lazy1] must be of type Lazy1, stdClass given
-- autoload runs for a positional tuple member (member 1) --
  autoload(Lazy2)
  Element 1 of tuple[int,Lazy2] must be of type Lazy2, stdClass given
-- autoloader throws: the element TypeError carries it as previous (nothing leaks) --
  autoload(Boom)
  TypeError <- previous RuntimeException: autoload failed
  autoload(Boom)
  tuple previous: autoload failed
-- a matching, already-loaded class validates with no autoload --
int(2)
bool(true)
bool(true)
int(5)
