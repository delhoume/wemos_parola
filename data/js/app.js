var url = "";

function setSpeed(range, component, value) {
    if (range != null)
        range.val(value);
    component.html("Scrolling speed (" + value + ")");
}

function setIntensity(range, component, value) {
    if (range != null)
        range.val(value);
    component.html("Intensity (" + value + ")");
}

$(document).ready(function() {
    var scrollingspeed = $("#scrollingspeed");
    var scrollingspeedtext = $("#scrollingspeedtext");
    var intensity = $("#intensity");
    var intensitytext = $("#intensitytext");
    
    var displayBitcoin = $("#displayBitcoin");
    // get current value
    /*
    $.get(url + "all", function(data) {
        // TODO: direct JSON access
        var json = JSON.parse(data);
        // ....
        $.each(data, function(index, field) {
            if (index == "speed") {
                setSpeed(scrollingspeed, scrollingspeedtext, field);
            } else if (index == "displayBitcoin) {
                displayBitcoin.value = field;
            }
        });
    */
    setSpeed(scrollingspeed, scrollingspeedtext, 70);
    setIntensity(intensity, intensitytext, 5);
    // does not work
    displayBitcoin.checked = true;

    scrollingspeed.on("change mousemove", function() {
       // post value
       //$.post(url + "speed?value=" + this.value);
       setSpeed(null, scrollingspeedtext, this.value);
    });
    intensity.on("change mousemove", function() {
        // post value
        //$.post(url + "speed?value=" + this.value);
        setIntensity(null, intensitytext, this.value);
     });
 
    displayBitcoin.on("change", function() {
        $.post(url + "/displayBitcoin?value=", this.value);
    });
})