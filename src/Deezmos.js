let dummy_input = document.createElement("input");
dummy_input.type = "file";
dummy_input.onchange = ({ target: { files: [dummy_blob] } }) =>
{
	let dummy_reader = new FileReader();
	dummy_reader.readAsText(dummy_blob, "UTF-8");
	dummy_reader.onload = ({ target: { result: file_content }}) =>
	{
		let json  = JSON.parse(file_content);
		let state = Calc.getState();

		if ("avg_screenshot_rgb_analysis" in json)
		{
			const collection_folder_id = Math.random().toString();

			state.graph.squareAxes = false;
			state.graph.viewport.xmin = -0.5;
			state.graph.viewport.xmax = Math.max(...json.avg_screenshot_rgb_analysis.map(analysis => analysis.samples.length)) + 1.25;
			state.graph.viewport.ymin = Math.min(...json.avg_screenshot_rgb_analysis.flatMap(analysis => analysis.samples.flatMap(sample => [sample.avg_rgb.r, sample.avg_rgb.g, sample.avg_rgb.b])));
			state.graph.viewport.ymax = Math.max(...json.avg_screenshot_rgb_analysis.flatMap(analysis => analysis.samples.flatMap(sample => [sample.avg_rgb.r, sample.avg_rgb.g, sample.avg_rgb.b])));

			[state.graph.viewport.ymin, state.graph.viewport.ymax] =
				[
					((state.graph.viewport.ymax + state.graph.viewport.ymin) / 2.0) - ((state.graph.viewport.ymax - state.graph.viewport.ymin) / 2.0) * 1.15,
					((state.graph.viewport.ymax + state.graph.viewport.ymin) / 2.0) + ((state.graph.viewport.ymax - state.graph.viewport.ymin) / 2.0) * 1.15,
				];

			const selector_string_constructor = analysis_index => `\\left\\{a_{collectionSelector} = 0, a_{collectionSelector} = ${analysis_index + 1} \\right\\}`;

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
										max     : json.avg_screenshot_rgb_analysis.length.toString(),
										step    : "1",
									}
							},
							{
								type : "expression",
							},
							...json.avg_screenshot_rgb_analysis.flatMap((analysis, analysis_index) => ([
								{
									type  : "expression",
									latex : `C_${analysis_index + 1} = \\left[\\operatorname{rgb}\\left(255, 0, 0\\right), \\operatorname{rgb}\\left(30, 150, 30\\right), \\operatorname{rgb}\\left(0, 0, 255\\right)\\right]`
								},
								{
									type      : "folder",
									title     : `Collection ${analysis_index + 1} ("${analysis.wildcard_path}").`,
									collapsed : true,
									id        : collection_folder_id,
								},
								...['R', 'G', 'B'].flatMap((channel_name, channel_index) => ([ // Average lines.
									{
										type       : "expression",
										folderId   : collection_folder_id,
										colorLatex : `C_${analysis_index + 1}\\left[${channel_index + 1}\\right]`,
										latex      : `y = \\operatorname{mean}\\left(${channel_name}_${analysis_index + 1}\\right) ${selector_string_constructor(analysis_index)}`,
									},
									...analysis.samples.flatMap((screenshot, screenshot_index) => ({ // Labeled sample points.
										type             : "expression",
										colorLatex       : `C_1\\left[${channel_index + 1}\\right]`,
										label            : screenshot.name,
										labelSize        : "0.6",
										labelOrientation : screenshot_index % 2 ? "above" : "below",
										showLabel        : true,
										folderId         : collection_folder_id,
										pointOpacity     : "a_{fileNameOpacity}",
										latex            : `\\left(${screenshot_index + 1}, ${channel_name}_${analysis_index + 1}\\left[${screenshot_index + 1}\\right]\\right) ${selector_string_constructor(analysis_index)}`
									})),
									...analysis.samples.flatMap((screenshot, screenshot_index) => ({ // Sample points.
										type             : "expression",
										colorLatex       : `C_1\\left[${channel_index + 1}\\right]`,
										folderId         : collection_folder_id,
										latex            : `\\left(${screenshot_index + 1}, ${channel_name}_${analysis_index + 1}\\left[${screenshot_index + 1}\\right]\\right) ${selector_string_constructor(analysis_index)}`
									})),
								])),
								{
									type     : "table",
									folderId : collection_folder_id,
									columns  :
									[
										{ latex: `I_${analysis_index + 1}`, values: analysis.samples.map((_, i) => (i + 1).toString()) },
										{ latex: `R_${analysis_index + 1}`, values: analysis.samples.map(screenshot => screenshot.avg_rgb.r.toString()), hidden: true },
										{ latex: `G_${analysis_index + 1}`, values: analysis.samples.map(screenshot => screenshot.avg_rgb.g.toString()), hidden: true },
										{ latex: `B_${analysis_index + 1}`, values: analysis.samples.map(screenshot => screenshot.avg_rgb.b.toString()), hidden: true },
									]
								},
								{
									type : "expression",
								}
							]))
						]
			};
		}

		Calc.setState(state);
	};
};
setTimeout(() => document.querySelector("div.align-right-container").appendChild(dummy_input), 100);
