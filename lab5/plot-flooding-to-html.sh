#!/bin/bash

#  Written by Chris.McDonald@uwa.edu.au

#  Location of cnet on your system
CNET="/usr/local/bin/cnet"


TOPOLOGIES="FLOODING1 FLOODING2 FLOODING3"

TITLE="Comparison of flooding-based routing algorithms"
XAXIS="seconds"

#YAXIS="number of AL messages delivered"
#METRIC="Messages delivered"

#YAXIS="number of PL frames transmitted"
#METRIC="Frames transmitted"

YAXIS="Delivery efficiency"
METRIC="efficiency"

DURATION="300secs"
EVERY="10secs"

HTML="plot.html"

# --  nothing needs changing below this line  --------------------------

function run_simulations() {
    for t in `echo $TOPOLOGIES`
    do
        rm -f stats-$t
        $CNET -WTq -e $DURATION -s -f $EVERY $t			| \
        grep -i "$METRIC"					| \
        cut -d: -f 2                                            | \
        awk "{ printf(\"%d %s\n\", ++i * $EVERY, \$1); }"       > stats-$t
    done
}

function collate_stats() {
    T0=`echo " " $TOPOLOGIES | sed -e 's/ / stats-/g' | cut '-d ' -f3-`
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

    for t in "$XAXIS" `echo $TOPOLOGIES` ; do \
	echo "data.addColumn('number', '$t');" ; done

    echo 'data.addRows(['
    eval $CMD | sed -e 's/ /, /g' -e 's/.*/  [&],/'
    echo ']);'
}

#  ref:  https://developers.google.com/chart/interactive/docs/gallery/linechart
function build_html() {
    cat << END_END
<html>
<head>
  <script type="text/javascript" src="https://www.gstatic.com/charts/loader.js"></script>
  <script type='text/javascript'>
    google.charts.load('current', {'packages':['corechart', 'line']});
    google.charts.setOnLoadCallback(drawChart);

    function drawChart() {
      var options = {
        title: '$TITLE',
        hAxis: { title: '$XAXIS' },
        vAxis: { title: '$YAXIS' },
        chartArea: {backgroundColor: '#eee', top: 30, left: 80, bottom: 50},
        width:  900,
        height: 500,
      };
      var chart = new google.visualization.LineChart(document.getElementById('myplot'));
      var data  = new google.visualization.DataTable();

END_END

    collate_stats | sed 's/^/      /'

    cat << END_END
      chart.draw(data, options);
    }
  </script>
</head>

<body>
  <div id='myplot'></div>
</body>
</html>
END_END
}

function remove_stats() {
    for t in `echo $TOPOLOGIES` ; do rm -f stats-$t ; done
}


rm -f $HTML
run_simulations
build_html > $HTML
remove_stats
echo "output is in $HTML"
