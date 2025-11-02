<script lang="ts">
  import { onMount } from "svelte";
  import type { Action } from "svelte/action";

  let wssUrl = $state(
    window.location
      .toString()
      .replace("/ui","/")
      .replace(/http(s?):\/\//, "wss://") // + "ws"
      .replace("localhost:5173", "192.168.4.1"), //update to match your EOM's IP address...useful for local testing
  ) as string;
  if ((window.location.toString().match(/\.\d+\/?$/) ?? []).length > 0) {
    window.location.replace(window.location.toString() + "ws");
  }

  type eomReading = {
    arousal: number;
    cooldown?: number; //boolean 1 or 0
    denied?: number;
    localTime?: number;
    millis: number;
    motor: number; //calling this "pleasure" now since it can change more than motors but leaving it backwards compatible
    pavg: number;
    pressure: number;
    runMode: string;
    sensitivity: number;
    threshold: number;
  };

  type settingType = {
    value: number;
    min: number;
    max: number;
    label: string;
    type?: string;
    description?: string;
  };

  type eomSettings = {
    sensitivity_threshold: settingType;
    vibration_mode: settingType;
    motor_max_speed: settingType;
    motor_ramp_time_s: settingType;
    motor_start_speed: settingType;
    edge_delay: settingType;
    max_additional_delay: settingType;
    minimum_on_time: settingType;
    pressure_smoothing: settingType;
    update_frequency_hz: settingType;
    sensor_sensitivity: settingType;
    clench_pressure_sensitivity: settingType;
    clench_time_to_orgasm_ms: settingType;
    clench_time_threshold_ms: settingType;
    clench_detector_in_edging: settingType;
    max_clench_duration_ms: settingType;
    auto_edging_duration_minutes: settingType;
    post_orgasm_duration_seconds: settingType;
    use_average_values: settingType;
    use_post_orgasm: settingType;
  };

  let vibration_modes: Record<number, string> = {
    //0: "GlobalSync",
    1: "RampStop",
    2: "Depletion",
    3: "Enhancement",
    //4: "Pattern",
  };
  // let operation_modes: Record<number, string> = {  //also known as runMode
  //   1: "manual",
  //   2: "automatic",
  //   //3: "orgasm",
  // };

  let runModes: string[] = ["MANUAL", "AUTOMATIC","ORGASM"];

  function getNumericValue(r: eomReading, lineType: string): number {
    if (lineType === "pleasure" || lineType === "motor") {
      let yScaledMotor = typeof r.motor === "number"
      ? Math.round(r.motor / settings.motor_max_speed.value * maxY)
      : 0;
      //console.log(yScaledMotor,r.motor,settings.motor_max_speed.value,maxY);
      return yScaledMotor;
    }
    
    if (lineType === "runMode" || lineType === "cooldown") return 0;
    if (lineType === "denials") { //using "denials" for the scaled value
      return typeof r.denied === "number" ? Math.round(r.denied / maxDenied * maxY) : 0;
    }
    const val = (r as any)[lineType];
    return typeof val === "number" ? val : 0;
  }

  function lastNumericValue(r: eomReading[], lineType: string): number {
    if (r.length === 0) return 0;
    const lastReading = r[r.length - 1];
    //const val = (lastReading as any)[lineType];
    return getNumericValue(lastReading, lineType);
  }

  let settings = $state({
    sensitivity_threshold: {
      value: 1000,
      min: 0,
      max: 4096,
      label: "Arousal Threshold",
      type: "%",
      description: "The arousal threshold for orgasm detection. Lower values stop sooner.",
    },
    vibration_mode: { 
      value: 2, 
      min: 1, 
      max: 4, 
      label: 
      "Mode",
      type: "mode",
      description: "The pleasure algorithm used during automatic stimulation.",
    },
    motor_max_speed: {
      value: 255,
      min: 0,
      max: 255,
      label: "Maximum Pleasure Intensity",
      type: "%",
      description: "Maximum intensity of pleasure during stimulation.",
    },
    motor_ramp_time_s: {
      value: 10,
      min: 0,
      max: 120,
      label: "Pleasure Ramp Time",
      type: "seconds",
      description: "Time (s) taken to ramp the pleasure from the starting amount to the maximum.",
    },
    motor_start_speed: {
      value: 10,
      min: 0,
      max: 255,
      label: "Starting Pleasure Intensity",
      type: "%",
      description: "Initial intensity of pleasure when stimulation starts.",
    },
    edge_delay: {
      value: 10,
      min: 0,
      max: 60,
      label: "Edge Delay",
      type: "seconds",
      description: "Minimum time to wait after edge detection before resuming stimulation.",
    },
    max_additional_delay: {
      value: 10,
      min: 0,
      max: 60,
      label: "Max Additional Delay",
      type: "seconds",
      description: "Maximum time (s) that can be added to the edge delay before resuming stimulation. A random number will be picked between 0 and this setting each cycle. 0 to disable.",
    },
    minimum_on_time: {
      value: 5,
      min: 0,
      max: 60,
      label: "Minimum On Time",
      type: "seconds",
      description: "Time after stimulation restarts before edge detection is resumed.",
    },
    pressure_smoothing: {
      value: 10,
      min: 0,
      max: 100,
      label: "Pressure Smoothing",
      type: "samples",
      description: "Number of samples to take an average of. Higher numbers can reduce false positives but may result in lag and lower resolution!",
    },
    update_frequency_hz: {
      value: 50,
      min: 1,
      max: 100,
      label: "Update Frequency (Hz)",
      type: "Hz",
      description: "Update frequency for pressure readings and arousal steps.",
    },
    sensor_sensitivity: {
      value: 16,
      min: 1,
      max: 255,
      label: "Sensor Sensitivity",
      type: "%",
      description: "Analog pressure sensor prescaling. Higher values are more sensitive.",
    },
    clench_pressure_sensitivity: {
      value: 3000,
      min: 0,
      max: 4096,
      label: "Clench Pressure Sensitivity",
      type: "",
      description: "Minimum additional Arousal level to detect clench",
    },
    clench_time_to_orgasm_ms: {
      value: 5000,
      min: 0,
      max: 60000,
      label: "Clench Time to Orgasm (ms)",
      type: "ms",
      description: "Threshold variable that is milliseconds count of clench to detect orgasm.",
    },
    clench_time_threshold_ms: {
      value: 2000,
      min: 0,
      max: 60000,
      label: "Clench Time Threshold (ms)",
      type: "ms",
      description: "Threshold variable that is milliseconds counts to detect the start of clench.",
    },
    clench_detector_in_edging: {
      value: 1,
      min: 0,
      max: 1,
      label: "Clench Detector in Edging",
      type: "bool",
      description: "Enable clench detection while in edging mode.",
    },
    max_clench_duration_ms: {
      value: 20000,
      min: 0,
      max: 60000,
      label: "Max Clench Duration (ms)",
      type: "ms",
      description: "Duration the clench detector can raise arousal if clench detector turned on in edging session.",
    },
    auto_edging_duration_minutes: {
      value: 15,
      min: 0,
      max: 60,
      label: "Auto Edging Duration",
      type: "minutes",
      description: "How long to edge before permiting an orgasm.",
    },
    post_orgasm_duration_seconds: {
      value: 60,
      min: 0,
      max: 180,
      label: "Post Orgasm Duration",
      type: "seconds",
      description: "How long to stimulate after orgasm detected.",
    },
    use_post_orgasm: { 
      value: 1, 
      min: 0, 
      max: 1, 
      label: "Use Post Orgasm",
      type: "bool",
      description: "Use post-orgasm torture mode and functionality.",
    },
    use_average_values: {
      value: 1,
      min: 0,
      max: 1,
      label: "Use Average Values",
      type: "bool",
      description: "Use average values when calculating arousal. This smooths noisy data.",
    },
  } as eomSettings);

  let mainSettings: string[] = [
    "sensitivity_threshold",
    "sensor_sensitivity",
    "motor_max_speed",
    "motor_ramp_time_s",
  ];

  let modalSettings: string[] = [
    "motor_start_speed",
    "edge_delay",
    "max_additional_delay",
    "minimum_on_time",
    "pressure_smoothing",
    "update_frequency_hz",
    "sensor_sensitivity",
    "clench_pressure_sensitivity",
    "clench_time_to_orgasm_ms",
    "clench_time_threshold_ms",
    "clench_detector_in_edging",
    "max_clench_duration_ms",
    "auto_edging_duration_minutes",
    "post_orgasm_duration_seconds",
    "use_post_orgasm",
    "use_average_values",
  ];

  //let threshold: number = $state(1000) as number;
  let readings = $state([
    {
      pressure: 0,
      arousal: 0,
      motor: 0,
      threshold: 0,
      denied: 0,
      runMode: "AUTOMATIC",
      localTime: 0,
    } as eomReading,
    {
      pressure: 4000,
      arousal: 4000,
      motor: 255,
      threshold: 400,
      denied: 10,
      runMode: "AUTOMATIC",
      localTime: Date.now(),
    } as eomReading,  
  ]) as eomReading[]; //initial dummy data

  let lines: Record<string, [string, number]> = {
    pressure: ["green", 1],
    pleasure: ["orange", 2],
    arousal: ["red", 2],
    threshold: ["red", 1],
    denials: ["#FF7FFF", 1],
  };

  let bgt: number = $state(0) as number;
  let chartTime = $state(5) as number;
  let chartCanvas: HTMLCanvasElement;
  let ctx: CanvasRenderingContext2D;
  // function getXTicks(): number[] {
  //   return Array.from({ length: chartTime + 1 }, (_, i) => chartTime - i);
  // }
  const padding = { top: 10, right: 0, bottom: 5, left: 5 };
  let maxY = 4096;
  let fudge: number = 2;
  let arousalThicknessMultiplier: number = 4;
  let currentPleasure = $state(0) as number;
  let maxDenied = $state(20) as number;

  let isConnected = $state(false) as boolean;

  let socket = $state() as WebSocket;

  let showModal = $state(false);
  let dialog = $state() as HTMLDialogElement;

  function chartReady() {
    console.log("Chart ready");
    ctx.canvas.width = window.innerWidth;
    ctx.canvas.height = window.innerHeight;
    const dataLines = Object.entries(lines);
    const keySpacing = Math.round(
      (chartCanvas.width -
        (chartCanvas.width * (padding.left + padding.right)) / 100) /
        (dataLines.length + 1),
    );
    let keyHeight = Math.round((ctx.canvas.height * padding.top) / 100); //use 3/4 of top padding area
    let keyWidth = keySpacing / 4;
    let keyPadding = keyWidth / 4;

    ctx.font = "bold 1.5em sans-serif";
    //ctx.textBaseline = "middle";
    ctx.fontStretch = "condensed";
    ctx.textRendering = "optimizeLegibility";
    ctx.fillStyle = "black";
    ctx.fillRect(0, 0, chartCanvas.width, chartCanvas.height);

    //y axis labels
    for (let y = 0; y <= maxY; y += maxY / 4) {
      const yPos =
        ctx.canvas.height -
        (y / maxY) *
          (ctx.canvas.height -
            (ctx.canvas.height * (padding.top + padding.bottom)) / 100) -
        (ctx.canvas.height * padding.bottom) / 100;
      ctx.fillStyle = "white";
      ctx.fillText(
        Math.round((y / maxY) * 100).toString() + "%",
        5,
        yPos,
        Math.round((chartCanvas.width * padding.left) / 100 / 1.5),
      );
    }

    //data keys
    let i: number = keySpacing;
    for (const [lineType, [color, width]] of dataLines) {
      ctx.fillStyle = color;
      ctx.fillRect(
        i,
        Math.round(((ctx.canvas.height * padding.top) / 100) * 1.25) -
          keyHeight,
        keyWidth,
        Math.round(keyHeight / 2),
      );
      ctx.fillText(
        lineType,
        i + keyWidth + keyPadding,
        keyHeight / 1.666,
        keySpacing - keyWidth - keyPadding,
      );
      console.log(
        "Drawing key:",
        lineType,
        i + keyWidth + keyPadding,
        Math.round((ctx.canvas.height * padding.top) / 100) - 20,
      );
      i += keySpacing;
    }

    updateChart();
  }

  function scaleLinear(domain: number[], range: number[]) {
    if (domain[1] === domain[0]) return (value: number) => 0;
    const m = (range[1] - range[0]) / (domain[1] - domain[0]);
    const minRange = Math.min(range[0], range[1]);
    const maxRange = Math.max(range[0], range[1]);
    return (value: number) =>
      Math.max(
        minRange,
        Math.min(maxRange, range[0] + m * (value - domain[0])),
      );
  }

  function isAroused(_reading: eomReading): boolean {
    return _reading.arousal >= _reading.threshold;
  }

  function isOnCooldown(_reading: eomReading): boolean {
    return (_reading.cooldown ?? 0) > 0 && _reading.motor == 0;
  }

  function updateChart() {
    if (!ctx || !chartCanvas) return;
    const _readings = $state.snapshot(readings);
    if (_readings.length === 0) return;
    const xScale = scaleLinear(
      [0, _readings.length - 1],
      [
        (chartCanvas.width * padding.left) / 100,
        chartCanvas.width - (chartCanvas.width * padding.right) / 100,
      ],
    );
    const yScale = scaleLinear(
      [0, maxY],
      [
        chartCanvas.height - (chartCanvas.height * padding.bottom) / 100,
        (chartCanvas.height * padding.top) / 100 + fudge,
      ],
    );

    //background
    if (
      isAroused(_readings[_readings.length - 1]) ||
      isOnCooldown(_readings[_readings.length - 1])
    ) {
      if (bgt <= 0) {
        ctx.fillStyle = ctx.fillStyle == "#111111" ? "#330000" : "#111111";
        bgt = 5;
      } else {
        bgt -= 1;
      }
    } else {
      ctx.fillStyle = "#111111";
    }
    ctx.fillRect(
      (chartCanvas.width * padding.left) / 100 - fudge,
      (chartCanvas.height * padding.top) / 100 - fudge,
      chartCanvas.width - (chartCanvas.width * padding.right) / 100,
      chartCanvas.height - (chartCanvas.height * padding.bottom) / 100,
    );

    //y axis guides
    for (let y = 0; y <= maxY; y += maxY / 4) {
      const yPos =
        ctx.canvas.height -
        (y / maxY) *
          (ctx.canvas.height -
            (ctx.canvas.height * (padding.top + padding.bottom)) / 100) -
        (ctx.canvas.height * padding.bottom) / 100;
      ctx.beginPath();
      ctx.setLineDash([5, 5]);
      ctx.strokeStyle = "white";
      ctx.moveTo((chartCanvas.width * padding.left) / 100, yPos);
      ctx.lineTo(
        chartCanvas.width - (chartCanvas.width * padding.right) / 100,
        yPos,
      );
      ctx.stroke();
    }

    //data lines
    for (const [lineType, [color, width]] of Object.entries(lines)) {
      ctx.strokeStyle = color;
      ctx.setLineDash(lineType === "threshold" ? [10, 10] : []); // Dashed line for threshold
      ctx.beginPath();
      ctx.moveTo(
        (chartCanvas.width * padding.left) / 100 + fudge,
        yScale(getNumericValue(_readings[0], lineType)),
      );
      _readings.forEach((r, i) => {
        if (isAroused(r) && lineType === "arousal") {
          ctx.lineWidth = width * arousalThicknessMultiplier; //thicker line when aroused
        } else {
          ctx.lineWidth = width;
        }
        ctx.lineTo(xScale(i), yScale(getNumericValue(r, lineType)));
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(xScale(i), yScale(getNumericValue(r, lineType)));
      });
    }
  }

  function handleChartTimeChange() {
    console.log(`Chart time changed: ${chartTime}`);
    updateChart();
  }

  function handleUpdated(e: CustomEvent<number[]>) {
    updateChart();
  }

  function handleConfigList(configList: Object) {
    console.log("Received configuration data:", configList);
    for (const [key, value] of Object.entries(configList)) {
      if (key === "sensitivity_threshold") {
        settings.sensitivity_threshold.value = Number(value);
      } else if (key === "motor_ramp_time_s") {
        settings.motor_ramp_time_s.value = Number(value);
      } else if (key === "motor_max_speed") {
        settings.motor_max_speed.value = Number(value);
      }
    }
  }

  function handleMessage(event: MessageEvent) {
    try {
      let wsMsg = JSON.parse(event.data);

      if (!wsMsg) {
        console.warn("Received empty data:", wsMsg);
        return;
      }
      if (!wsMsg.readings) {
        if (wsMsg.wifiStatus) {
          //console.log("Received WiFi status:", wsMsg.wifiStatus);
        } else if (wsMsg.configList) {
          handleConfigList(wsMsg.configList);
        } else if (
          wsMsg.setMode !== undefined ||
          wsMsg.setMotor !== undefined ||
          wsMsg.vibration_mode !== undefined
        ) {
          //console.log("Received ack for setting change:", wsMsg);
        } else {
          console.warn("Received unknown data:", wsMsg);
        }
        return;
      }
      wsMsg = wsMsg.readings;

      if (!wsMsg.millis) {
        console.warn("Received data without 'millis' field:", wsMsg);
        return;
      }
      if (
        wsMsg.pressure === undefined ||
        wsMsg.arousal === undefined ||
        wsMsg.motor === undefined
      ) {
        console.warn("Received data missing expected fields:", wsMsg);
        return;
      }
      wsMsg.localTime = Date.now();
      readings.push(wsMsg);
      //currentPleasure = getNumericValue(wsMsg, "pleasure");
      while (
        readings.length > 0 &&
          ((wsMsg.millis - (readings[0].millis ?? 0) > chartTime * 1000) ||  //the first and last reading are more than chartTime seconds apart
          (Date.now() - (readings[0].localTime ?? 0) > chartTime * 1000))  //the first reading is older than chartTime seconds
      ) {
        readings.shift();
      }

      chartCanvas?.dispatchEvent(new CustomEvent("updated", {}));
    } catch (error) {
      console.error("Error parsing WebSocket message:", error);
    }
  }

  function handleOpen() {
    isConnected = true;
    console.log("WebSocket connection established");
    let msg = JSON.stringify({ configList: null });
    socket.send(msg);
    socket.send(JSON.stringify({ streamReadings: null }));
    chartCanvas?.dispatchEvent(new CustomEvent("ready", {}));

    setInterval(() => {
      let lastReadingTime = $state.snapshot(readings)[$state.snapshot(readings).length - 1].localTime ?? 0;
      if (Date.now() - lastReadingTime > 10000) { // 10 seconds threshold
        console.log("WebSocket data is stale...dropped connection?");
        isConnected = false;
        socket.close();
        socket = new WebSocket(wssUrl);
        initializeWebSocket(socket);
        console.log("Reconnecting to ", wssUrl, socket);
      }
    }, 2000); // Check for a dropped connection every 2 seconds

  }

  function handleError(event: Event) {
    isConnected = false;
    console.error("WebSocket error observed:", event);
  }

  function handleSettingChange(setting_name: string, value: number) {
    settings[setting_name as keyof typeof settings].value = value;
    let msg = JSON.stringify({
      configSet: { [setting_name]: value },
    });
    socket.send(msg);
    console.log(`Sent ${setting_name} change:`, value);
  }

  function handleBasicChange(setting_name: string, value: number | string | null) {
    let msg = JSON.stringify({
      [setting_name]: value,
    });
    socket.send(msg);
    console.log(`Sent ${setting_name} change:`, value);
  }

  let debounceTimeouts = new Map<string, ReturnType<typeof setTimeout>>();
  const DEBOUNCE_DELAY = 20; //  delay in ms

  // This needs work
  function debouncedBasicChange(setting_name: string, value: number | string) {
    // Clear existing timeout for this setting
    const existingTimeout = debounceTimeouts.get(setting_name);
    if (existingTimeout !== undefined) {
      clearTimeout(existingTimeout);
    }
    
    // Set new timeout
    const timeoutId = setTimeout(() => {
      handleBasicChange(setting_name, value);
      debounceTimeouts.delete(setting_name);
    }, DEBOUNCE_DELAY);
    
    debounceTimeouts.set(setting_name, timeoutId);
  }

  function initializeWebSocket(socket: WebSocket) {
    socket.onopen = handleOpen;
    socket.onerror = handleError;
    socket.onmessage = handleMessage;
  }

  const chart: Action<
    HTMLCanvasElement,
    undefined,
    {
      onready: (e: CustomEvent<void>) => void;
      onupdated: (e: CustomEvent<number[]>) => void;
    }
  > = (canvas) => {
    chartCanvas = canvas;
    ctx = chartCanvas.getContext("2d") as CanvasRenderingContext2D;

    function dispatch<T>(name: string, detail: T) {
      chartCanvas.dispatchEvent(new CustomEvent(name, { detail }));
    }
    onMount(() => {
      dispatch("ready", undefined);
    });
  };

  onMount(() => {
    socket = new WebSocket(wssUrl);
    initializeWebSocket(socket);

    setInterval(() => {
      if (!isConnected) {
        console.log("WebSocket is not connected");
        return;
      }
      console.log(
        "Current readings:",
        $state.snapshot(readings)[$state.snapshot(readings).length - 1],
      );
    }, 30000); // Log every 30 seconds
  });


</script>

{#snippet InputSlider(id: string)}
  <div style="width: 100%;">
    <label for={id} title="{settings[id as keyof typeof settings].description}">
      {settings[id as keyof typeof settings].label} :
      {settings[id as keyof typeof settings].type === "%"
        ? Math.round(
            (settings[id as keyof typeof settings].value /
              settings[id as keyof typeof settings].max) *
              100,
          ).toString() + " %"
        : settings[id as keyof typeof settings].value.toString() +
          " " +
          settings[id as keyof typeof settings].type}
    </label>
    <input
      {id}
      title="{settings[id as keyof typeof settings].description}"
      type="range"
      min={settings[id as keyof typeof settings].min}
      max={settings[id as keyof typeof settings].max}
      bind:value={settings[id as keyof typeof settings].value}
      style="width: 100%;"
      onchange={() =>
        handleSettingChange(id, settings[id as keyof typeof settings].value)}
    />
  </div>
{/snippet}

<div
  class="mainContainer"
  onclick={(e) => {
    if (e.target !== dialog) dialog.close();
  }}
  onkeyup={(e) => {
    if (e.key === "Escape") dialog.close();
  }}
  role="none"
>
  <div style="position: relative; padding: .5%; width:100%;">
    <div style="display: flex; justify-content:space-between;">
      <div style="white-space: nowrap;">
        <input
          id="url"
          type="text"
          bind:value={wssUrl}
          style="margin-bottom: .5%;"
        /><input
          type="button"
          value="Connect"
          onclick={() => {
            wssUrl = wssUrl.trim();
            socket?.close();
            isConnected = false;
            socket = new WebSocket(wssUrl);
            initializeWebSocket(socket);
            console.log("Connecting to ", wssUrl, socket);
          }}
        />
        {#if isConnected}
          <span style="color: green; font-size: small;"> ● Connected</span>
        {:else}
          <span style="color: red; font-size: small;"> ● Disconnected</span>
        {/if}
      </div>  
      <button
        onclick={() => (showModal = true)}
        style="cursor: pointer; margin: .2%;padding-bottom: 0;"
        aria-label="Open settings"
        ><svg
          xmlns="http://www.w3.org/2000/svg"
          width="20"
          height="20"
          viewBox="0 0 24 24"
          cursor="pointer"
          aria-label="Settings"
        >
          <path
            fill="#ccc"
            d="M24 13.616v-3.232c-1.651-.587-2.694-.752-3.219-2.019v-.001c-.527-1.271.1-2.134.847-3.707l-2.285-2.285c-1.561.742-2.433 1.375-3.707.847h-.001c-1.269-.526-1.435-1.576-2.019-3.219h-3.232c-.582 1.635-.749 2.692-2.019 3.219h-.001c-1.271.528-2.132-.098-3.707-.847l-2.285 2.285c.745 1.568 1.375 2.434.847 3.707-.527 1.271-1.584 1.438-3.219 2.02v3.232c1.632.58 2.692.749 3.219 2.019.53 1.282-.114 2.166-.847 3.707l2.285 2.286c1.562-.743 2.434-1.375 3.707-.847h.001c1.27.526 1.436 1.579 2.019 3.219h3.232c.582-1.636.75-2.69 2.027-3.222h.001c1.262-.524 2.12.101 3.698.851l2.285-2.286c-.744-1.563-1.375-2.433-.848-3.706.527-1.271 1.588-1.44 3.221-2.021zm-12 2.384c-2.209 0-4-1.791-4-4s1.791-4 4-4 4 1.791 4 4-1.791 4-4 4z"
          />
        </svg>
      </button>
    </div>
    <canvas
      style="position: relative; height:40vh; width:100%; "
      bind:this={chartCanvas}
      use:chart
      onupdated={handleUpdated}
      onready={chartReady}
      onresize={chartReady}
    ></canvas>

    <div class="statBoxes">
      <div class="statBoxButton" title="Current pressure level"
          onclick={() => {
          //cant think of an action for clicking pressure yet
        }}
      style="border-color: green; color: green;" role="none"
      >
        Pressure: {Math.round(lastNumericValue($state.snapshot(readings), "pressure") / maxY * 100)} %
      </div>

      <div class="statBoxButton" title="Click to halt pleasure immediately!"
        onclick={() => {
          handleBasicChange("setMode", "manual");
          handleBasicChange("setMotor", 0);
          currentPleasure = 0;
        }}
        style="border-color: orange; color: orange;" role="none"
      >
        Pleasure: {Math.floor(
          (lastNumericValue($state.snapshot(readings), "pleasure") / maxY) *
            100,
        )
          ? Math.round(
              (lastNumericValue($state.snapshot(readings), "pleasure") / maxY) *
                99,
            ).toString() + "%"
          : "DENIED!"}
      </div>
      
      <div class="statBoxButton" title="Click to trigger a cooldown period"
        onclick={() => {
          handleBasicChange("triggerArousal", null);
        }}
        style="border-color: red; color: red;" role="none"
      >
        Arousal: {Math.floor(
          (lastNumericValue($state.snapshot(readings), "arousal") / maxY) * 100,
        )} %
      </div>

      <div class="statBoxButton" title="Click to reset denied orgasms count"
          onclick={() => {
          handleBasicChange("resetDenied", null);
        }}
      style="border-color: #FF7FFF; color: #FF7FFF;" role="none"
      >
        Denied: {lastNumericValue($state.snapshot(readings), "denied")}
      </div>


    </div>

    <div class="radioBoxes">
      <div
        class="radioBox"
      >
        <div style="font-weight: bold; margin-right: 2em;">Control:</div>
        {#each runModes as runMode, index}
          <div class="radioBoxItem">
            <input
              id="runmode-{index}"
              type="radio"
              onchange={() =>{
                handleBasicChange("setMode", runMode);
                currentPleasure = $state.snapshot(readings)[$state.snapshot(readings).length - 1].motor;
              }}
              name="runmode"
              value={runMode}
              checked={$state.snapshot(readings)[$state.snapshot(readings).length - 1].runMode == runMode}
            />
            <label for="runmode-{index}" class={$state.snapshot(readings)[$state.snapshot(readings).length - 1].runMode == runMode ? "radioChosen" : "radioChoice"}>{runMode.substring(0, 1).toUpperCase() + runMode.substring(1).toLowerCase()}</label>
          </div>
        {/each}
      </div>
      <div
        class="radioBox"
      >
        <div style="font-weight: bold; margin-right: 0.5em;">Mode:</div>
        {#each Object.entries(vibration_modes) as [modeId, modeName], index}
          <div class="radioBoxItem">
            <input
              id="vibemode-{modeId}"
              type="radio"
              onchange={() =>
                handleSettingChange("vibration_mode", Number(modeId))}
              name="vibemode"
              value={modeId}
              checked={settings.vibration_mode.value === Number(modeId)}
            />
            <label for="vibemode-{modeId}" class={settings.vibration_mode.value === Number(modeId) ? "radioChosen" : "radioChoice"} >{modeName}</label>
          </div>
        {/each}
      </div>
    </div>



    {#if $state.snapshot(readings)[$state.snapshot(readings).length - 1].runMode === 'MANUAL'}
    <label for="pleasure" style="color:orange">Pleasure: {Math.round(
              (lastNumericValue($state.snapshot(readings), "pleasure") / maxY) *
                99,
            )} %</label>
    <input
      id="currentPleasure"
      type="range"
      min="0"
      max={settings.motor_max_speed.value}
      step="1"
      bind:value={currentPleasure}
      style="width: 100%;accent-color: orange; background: linear-gradient(to right, orange 0%, orange 100%);"
      oninput={() => {
          //console.log("Dragging pleasure to ", currentPleasure);
          debouncedBasicChange("setMotor",currentPleasure )
          //handleBasicChange("setMotor",currentPleasure )
        }
      }
    />
      
    {/if}

    {#each mainSettings as settingId}
      {@render InputSlider(settingId)}
    {/each}

    <label for="chartTime">Time Window of Chart: {chartTime} seconds</label>
    <input
      id="chartTime"
      type="range"
      min="1"
      max="60"
      step="1"
      bind:value={chartTime}
      style="width: 100%;"
      onchange={handleChartTimeChange}
    />
  </div>
</div>

<dialog
  style="position: absolute; top: 10%; width: 80%; margin: auto;padding: 1em;  vertical-align: top;"
  open={showModal}
  bind:this={dialog}
  onclose={() => (showModal = false)}
>
  <div style="padding: 0; margin-top: 0;">
    <button
      style="float:right; font-size: 1.5em; background: none; border: none; cursor: pointer;"
      aria-label="Close settings"
      onclick={() => dialog.close()}
    >
      &times;
    </button>
    <h2>Settings</h2>
    <hr />
    <label for="chartTime">Maximum Denied Orgasms on Chart: {maxDenied} denied orgasms</label>
    <input
      title="This changes how many denied orgasms it takes to reach 100% on its chart line."
      id="maxDenied"
      type="range"
      min="1"
      max="100"
      step="1"
      bind:value={maxDenied}
      style="width: 100%;"
      onchange={handleChartTimeChange}
    />

    {#each modalSettings as settingId}
      {@render InputSlider(settingId)}
    {/each}
  </div>
</dialog>
