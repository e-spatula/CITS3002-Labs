#!/bin/bash

# location of the cnet simulator
CNET="/usr/local/bin/cnet"

# some variables to define our simulation:
TOPOLOGY="STOPANDWAIT2"
METRIC="Messages delivered"
DURATION="7200s"
EVERY="10"

# some variables to define our output plot:
HTML="plot.html"
TITLE="Number of messages delivered"
XAXIS="seconds"


# --  nothing needs changing below this line  --------------------------

if [ ! -x $CNET ]
then
    echo "$CNET not found or is not executable"
    exit 1
fi

function build_html() {
    cat << END_END
<html>
<head>
<!--
  <meta http-equiv='refresh' content='10' />
-->
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

      data.addColumn('number', 'seconds');
      data.addColumn('number', '$TOPOLOGY');

      data.addRows([
END_END

# the actual simulation, and capturing of the statistics, are done here:
    $CNET -W -q -T -e $DURATION -s -f ${EVERY}secs $TOPOLOGY	| \
    grep "$METRIC"						| \
    cut -d: -f 2						| \
    awk "{ printf(\"        [%d, %s],\n\", ++i * $EVERY, \$1); }"

cat << END_END
      ]);
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
build_html > $HTML
echo "output is in $HTML"
