#!/bin/bash

# location of the cnet simulator
CNET="/usr/local/bin/cnet"

TOPOLOGIES="STOPANDWAIT4 PIGGYBACK"

HTML="plot.html"

TITLE="Messages delivered in 1 hour"
XAXIS="seconds"

METRIC="Messages delivered"
DURATION="1hour"
EVERY="10"

# --  nothing needs changing below this line  --------------------------

function run_simulations() {
    for t in `echo $TOPOLOGIES`
    do
        rm -f stats.$t
        $CNET -W -q -T -e $DURATION -s -f ${EVERY}secs $t       | \
        grep "$METRIC"                                          | \
        cut -d: -f 2                                            | \
        awk "{ printf(\"%d %s\n\", ++i * $EVERY, \$1); }"       > stats.$t
    done
}

function collate_stats() {
    echo 'data.addRows(['

    T0=`echo " " $TOPOLOGIES | sed -e 's/ / stats./g' | cut '-d ' -f3-`
    T1=`echo $T0 | cut '-d ' -f1`
    T2=`echo $T0 | cut '-d ' -f1,2`

    if [ "$T0" == "$T1" ]
    then
	CMD="cat $T0"
    elif [ "$T0" == "$T2" ]
    then
	CMD="join $T0"
    else
	CMD="join "`echo $T0 | cut '-d ' -f1`
	CMD="$CMD "`echo $T0 | cut '-d ' -f2- | sed 's/ / | join - /g'`
    fi

    eval $CMD | sed -e 's/ /, /g' -e 's/.*/[&],/'
    echo ']);'
}

function add_titles() {
    for t in $XAXIS `echo $TOPOLOGIES`
    do
        echo "data.addColumn('number', '$t');"
    done
}

function build_html() {
    cat << END_END
<html>
<head>
  <script type='text/javascript' src='https://www.google.com/jsapi'></script>
  <script type='text/javascript'>
    google.load('visualization', '1.1', {packages: ['line']});
    google.setOnLoadCallback(drawChart);

    function drawChart() {
      var options = {
        chart: {
          title: '$TITLE'
        },
        width:  600,
        height: 400
      };
      var chart = new google.charts.Line(document.getElementById('linechart_material'));
      var data = new google.visualization.DataTable();
END_END

add_titles
collate_stats

cat << END_END
      chart.draw(data, options);
    }
  </script>
</head>
<body>
  <div id='linechart_material'></div>
</body>
</html>
END_END
}



rm -f $HTML
run_simulations
build_html > $HTML
echo "output is in $HTML"
