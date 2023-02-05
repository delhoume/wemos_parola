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
    
    $.get(url + "all", function(data) {
        alert(data);
        // TODO: direct JSON access
        var obj = JSON.parse(data);
        // ....
        setSpeed(scrollingspeed, scrollingspeedtext, obj.speed);
        setIntensity(intensity, intensitytext, obj.intensity);
    });
    
    //setSpeed(scrollingspeed, scrollingspeedtext, 70);
    //setIntensity(intensity, intensitytext, 5);
    // does not work
    displayBitcoin.checked = true;

    scrollingspeed.on("change", function() {
       delayPostValue("speed", this.value);
       setSpeed(null, scrollingspeedtext, this.value);
    });
    intensity.on("change", function() {
        delayPostValue("intensity", this.value);
        setIntensity(null, intensitytext, this.value);
     });
 
    displayBitcoin.on("change", function() {
        $.post(url + "displayBitcoin?value=", this.value);
    });
})

function delayPostValue(name, value) {
    clearTimeout(postValueTimer);
    postValueTimer = setTimeout(function() {
      postValue(name, value);
    }, 300);
  }

  function postValue(name, value) {
    var body = { name: name, value: value };
    $.post(url + name + "?value=" + value, body);
  }