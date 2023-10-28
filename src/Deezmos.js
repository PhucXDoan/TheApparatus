const foldered = (title, ...xs) =>
{
	let id = Math.random().toString();
	xs.forEach(x => { x.folderId = id });
	xs.unshift({ type : "folder", collapsed : true, id, title });
	return xs;
};

const RGB  = (r, g, b) => `\\operatorname{rgb}\\left(${r}, ${g}, ${b}\\right)`;
const MEAN = (...xs) => `\\operatorname{mean}\\left(${xs.join(", ")}\\right)`;

const dropdown_options =
	{
		avg_slot_rgbs       : "Average Slot RGBs",
		avg_screenshot_rgbs : "Average Screenshot RGBs",
	};

let dropdown_input = document.createElement("select");
for (let i in dropdown_options)
{
	let opt = document.createElement("option");
	opt.innerText = dropdown_options[i];
	dropdown_input.appendChild(opt);
}

let file_input = document.createElement("input");
file_input.type = "file";
file_input.onchange = ({ target: { files: [dummy_blob] } }) =>
{
	let dummy_reader = new FileReader();
	dummy_reader.readAsText(dummy_blob, "UTF-8");
	dummy_reader.onload = ({ target: { result: file_content }}) =>
	{
		let json  = JSON.parse(file_content);
		let state = Calc.getState();

		switch (dropdown_input.value)
		{
			case dropdown_options.avg_screenshot_rgbs:
			{
				state.graph.squareAxes = false;
				state.graph.viewport.xmin = -0.75;
				state.graph.viewport.xmax = Math.max(...json.screenshot_analysis.map(collection => collection.samples.length)) + 1.25;
				state.graph.viewport.ymin = Math.min(...json.screenshot_analysis.flatMap(collection => collection.samples.flatMap(sample => [sample.avg_rgb.r, sample.avg_rgb.g, sample.avg_rgb.b])));
				state.graph.viewport.ymax = Math.max(...json.screenshot_analysis.flatMap(collection => collection.samples.flatMap(sample => [sample.avg_rgb.r, sample.avg_rgb.g, sample.avg_rgb.b])));

				[state.graph.viewport.ymin, state.graph.viewport.ymax] =
					[
						((state.graph.viewport.ymax + state.graph.viewport.ymin) / 2.0) - ((state.graph.viewport.ymax - state.graph.viewport.ymin) / 2.0) * 1.15,
						((state.graph.viewport.ymax + state.graph.viewport.ymin) / 2.0) + ((state.graph.viewport.ymax - state.graph.viewport.ymin) / 2.0) * 1.15,
					];

				const selector_string_constructor = collection_index => `\\left\\{a_{collectionSelector} = 0, a_{collectionSelector} = ${collection_index + 1} \\right\\}`;

				state.expressions =
					{
						list:
							[
								{
									type   : "expression",
									latex  : "a_{fileNameOpacity} = 1",
									slider :
										{
											hardMin : true,
											hardMax : true,
											min     : "0",
											max     : "1",
										}
								},
								{
									type   : "expression",
									latex  : "a_{collectionSelector} = 0",
									slider :
										{
											hardMin : true,
											hardMax : true,
											min     : "0",
											max     : json.screenshot_analysis.length.toString(),
											step    : "1",
										}
								},
								{
									type : "expression",
								},
								...json.screenshot_analysis.flatMap((collection, collection_index) => ([
									{
										type  : "expression",
										latex : `C_${collection_index + 1} = \\left[${RGB(255, 0, 0)}, ${RGB(30, 150, 30)}, ${RGB(0, 0, 255)}\\right]`
									},
									...foldered
									(
										`Collection ${collection_index + 1} ("${collection.wildcard_path}").`,
										{ // Color.
											type        : "expression",
											colorLatex  : RGB(MEAN(`256R_{${collection_index + 1}}`), MEAN(`256G_{${collection_index + 1}}`), MEAN(`256B_{${collection_index + 1}}`)),
											lineOpacity : `\\left\\{a_{collectionSelector} = ${collection_index + 1} : 1, 0\\right\\}`,
											lineWidth   : "20",
											latex       : `x = 0.5`,
										},
										...['R', 'G', 'B'].flatMap((channel_name, channel_index) => ([
											{ // Average lines.
												type       : "expression",
												colorLatex : `C_${collection_index + 1}\\left[${channel_index + 1}\\right]`,
												latex      : `y = ${MEAN(channel_name + "_" + (collection_index + 1))} ${selector_string_constructor(collection_index)} \\left\\{x \\ge 0\\right\\}`,
											},
											...collection.samples.flatMap((screenshot, screenshot_index) => ([
												{ // Labeled sample points.
													type             : "expression",
													colorLatex       : `C_${collection_index + 1}\\left[${channel_index + 1}\\right]`,
													label            : screenshot.name,
													labelSize        : "0.6",
													labelOrientation : screenshot_index % 2 ? "above" : "below",
													showLabel        : true,
													pointOpacity     : "a_{fileNameOpacity}",
													latex            : `\\left(${screenshot_index + 1}, ${channel_name}_${collection_index + 1}\\left[${screenshot_index + 1}\\right]\\right) ${selector_string_constructor(collection_index)}`
												},
												{ // Sample points.
													type             : "expression",
													colorLatex       : `C_${collection_index + 1}\\left[${channel_index + 1}\\right]`,
													latex            : `\\left(${screenshot_index + 1}, ${channel_name}_${collection_index + 1}\\left[${screenshot_index + 1}\\right]\\right) ${selector_string_constructor(collection_index)}`
												}
											])),
										])),
										{
											type     : "table",
											columns  :
											[
												{ latex: `I_${collection_index + 1}`, values: collection.samples.map((_, i) => (i + 1).toString()) },
												{ latex: `R_${collection_index + 1}`, values: collection.samples.map(screenshot => screenshot.avg_rgb.r.toString()), hidden: true },
												{ latex: `G_${collection_index + 1}`, values: collection.samples.map(screenshot => screenshot.avg_rgb.g.toString()), hidden: true },
												{ latex: `B_${collection_index + 1}`, values: collection.samples.map(screenshot => screenshot.avg_rgb.b.toString()), hidden: true },
											]
										}
									),
									{ type : "expression" }
								]))
							]
				};
			} break;

			case dropdown_options.avg_slot_rgbs:
			{
			} break;
		}

		Calc.setState(state);
	};
};
setTimeout(() => {
	document.querySelector("div.align-right-container").appendChild(dropdown_input);
	document.querySelector("div.align-right-container").appendChild(file_input);
}, 100);
