<?php
/* One benchmark case in a fresh process. Usage: php -n runner.php '<json spec>' */
if (!function_exists('zend_test_vec_spike_bench')) {
    fwrite(STDERR, "zend_test_vec_spike_bench missing (build with --enable-zend-test)\n");
    exit(2);
}
$spec = json_decode($argv[1], true);
if (!is_array($spec)) {
    fwrite(STDERR, "bad spec json\n");
    exit(2);
}
echo json_encode(zend_test_vec_spike_bench($spec));
