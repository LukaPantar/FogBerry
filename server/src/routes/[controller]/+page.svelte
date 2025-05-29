
<script lang="ts">

    import RaspberryPiIcon from "$lib/icons/RaspberryPiIcon.svelte";
    import {goto} from "$app/navigation";

    let { data } = $props();


    function unixToDateString(unixTimestamp: number): string {
        const date = new Date(unixTimestamp);
        return date.toLocaleString();
    }

    // Transform the data to group by timestamp
    function transformSensorData(data) {
        const readingTypes = Object.keys(data.readings);
        const groupedByTimestamp = {};

        // Group all readings by timestamp
        readingTypes.forEach(readingType => {
            data.readings[readingType].forEach(reading => {
                const timestamp = reading.timestamp;
                if (!groupedByTimestamp[timestamp]) {
                    groupedByTimestamp[timestamp] = {
                        timestamp: timestamp,
                        time_unix: reading.time_unix,
                        id: reading.id
                    };
                }
                groupedByTimestamp[timestamp][readingType] = reading.value;
            });
        });

        // Convert to array and sort by timestamp
        return Object.values(groupedByTimestamp).sort((a, b) => b.time_unix - a.time_unix);
    }

    const sensors = data.sensors;
    sensors.forEach(s => {
        s.transformed = transformSensorData(s);
        s.types = Object.keys(s.readings);

    })

</script>

<style>

    article {
        display: flex;
        flex-wrap: wrap;
        gap: 20px;
    }


    .card {
        flex: 1 1 300px;
        background-color: var(--bg-primary);
        border-radius: 15px;
        border: 3px solid var(--bg-secondary);
        padding: 12px;
        display: flex;
        flex-direction: column;
        align-items: start;
    }

    .full-width {
        flex: 1 1 100%;
    }




    .table-container {
        overflow-x: auto;
        border-radius: 8px;
        width: 100%;
        border: 3px solid var(--bg-secondary);
        background: var(--bg-primary);
    }

    table {
        width: 100%;

        border-collapse: collapse;
        background: var(--bg-primary);
    }

    th {
        background: var(--bg-secondary);
        color: var(--primary);
        font-weight: 600;
        padding: 12px 10px;
        text-align: left;
        border-right: 1px solid var(--bg-secondary);
        border-bottom: 2px solid var(--accent);
        font-size: 14px;
        white-space: nowrap;
    }

    th:last-child {
        border-right: none;
    }

    td {
        padding: 10px;
        border-bottom: 1px solid var(--bg-secondary);
        border-right: 1px solid var(--bg-secondary);
        font-size: 13px;
        text-align: center;
        transition: all 0.2s ease;
        color: var(--primary);
    }

    td:last-child {
        border-right: none;
    }

    .timestamp {
        text-align: left;
        font-family: monospace;
        color: var(--secondary);
        min-width: 180px;
    }

    tr:hover {
        background-color: var(--bg-secondary);
        transform: translateY(-1px);
    }

    tr:hover td {
        border-color: var(--accent);
    }

    tr:nth-child(even) {
        background-color: rgba(0, 0, 0, 0.02);
    }

    [color-scheme='dark'] tr:nth-child(even) {
        background-color: rgba(255, 255, 255, 0.02);
    }

    /* Responsive design */
    @media (max-width: 768px) {
        .sensor-dashboard {
            padding: 10px;
        }

        th, td {
            padding: 8px 4px;
            font-size: 12px;
        }

        .timestamp {
            min-width: 140px;
        }
    }


</style>

<article>
    <div class="card full-width">
        <h2>Microcontroller: {data.controller}</h2>

        <span><b>First reading:</b> {unixToDateString(data.firstReading)}</span>
        <span><b>Last reading:</b> {unixToDateString(data.lastReading)}</span>

        <br/>

        <h4 style="margin-bottom: 0px">Sensors:</h4>
        <ul>
            {#each data.sensors as sensor}
                <li>{sensor.name}</li>
            {/each}
        </ul>


    </div>
    {#each data.sensors as sensor}
        <div class="card full-width">
            <h3>Sensor: {sensor.name}</h3>

            <h4>Last 20 readings:</h4>
            <div class="table-container">

            <table>
                <thead>
                <tr>
                    <th>Timestamp</th>
                    {#each sensor.types as readingType}
                        <th>{readingType.replace('_', ' ').toUpperCase()}</th>
                    {/each}
                </tr>
                </thead>
                <tbody>
                {#each sensor.transformed as reading}
                    <tr>
                        <td class="timestamp">{unixToDateString(reading.timestamp)}</td>
                        {#each sensor.types as readingType}
                            <td>{reading[readingType] || '-'}</td>
                        {/each}
                    </tr>
                {/each}
                </tbody>
            </table>
            </div>

            <!--<h4>Graph:</h4>-->
        </div>

    {/each}

</article>

